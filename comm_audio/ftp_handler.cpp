/*-------------------------------------------------------------------------------------
--	SOURCE FILE: ftp_handler.cpp - Contains ftp functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_ftp(SOCKET* socket, WSAEVENT ftp_packet_recved);
--					void open_file(std::string filename);
--					void create_new_file(std::string filename);
--					void write_file(char* data, int length);
--					void start_sending_file();
--					void start_receiving_file(int type, LPCWSTR request);
--					DWORD WINAPI ReceiveFileThreadFunc(LPVOID lpParameter);
--					void CALLBACK FTP_ReceiveRoutine(DWORD Error, DWORD BytesTransferred,
--											LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred,
--											LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--
--------------------------------------------------------------------------------------*/
#include "ftp_handler.h"

using namespace std;

FILE* requested_file;
FILE* file_output;
DWORD SendBytes;

LPSOCKET_INFORMATION FtpSocketInfo;
BOOL isReceivingFile = FALSE;

char ftp_complete_packet[1];
char file_not_found_packet[1];
char *buf;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_ftp
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_ftp(SOCKET* socket, WSAEVENT ftpCompletedEvent)
--									LPCWSTR tcp_port - port number for tcp
--									LPCWSTR udp_port - port number for udp
--									LPCWSTR svr_ip_addr - hostname/ip of server
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize the ftp feature on client side
--------------------------------------------------------------------------------------*/
void initialize_ftp(SOCKET* socket, WSAEVENT ftpCompletedEvent)
{
	buf = (char*)malloc(FTP_PACKET_SIZE);

	ftp_complete_packet[0] = TRANSFER_COMPLETE;
	file_not_found_packet[0] = FILE_NOT_FOUND;

	// Create a socket information structure to associate with the accepted socket.
	if ((FtpSocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		update_client_msgs("GlobalAlloc failed with error");
		return;
	}

	// Fill in the details of the server socket to receive file from
	FtpSocketInfo->Socket = *socket;
	ZeroMemory(&(FtpSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	FtpSocketInfo->BytesSEND = 0;
	FtpSocketInfo->BytesRECV = 0;
	FtpSocketInfo->DataBuf.len = FTP_PACKET_SIZE;
	FtpSocketInfo->DataBuf.buf = FtpSocketInfo->FTP_BUFFER;
	if (ftpCompletedEvent != NULL)
	{
		FtpSocketInfo->CompletedEvent = ftpCompletedEvent;
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	open_file
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void open_file(std::string filename)
--									std::string filename - name of the file to open
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to open a file for reading in binary
--------------------------------------------------------------------------------------*/
int open_file(std::string filename)
{
	return fopen_s(&requested_file, filename.c_str(), "rb");
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	open_file
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void create_new_file(std::string filename)
--									std::string filename - name of the file to create
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to create a new file with the supplied filename in binary
--------------------------------------------------------------------------------------*/
void create_new_file(std::string filename)
{
	fopen_s(&file_output, filename.c_str(), "wb");
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	close_file
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void close_file()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to close an open file
--------------------------------------------------------------------------------------*/
void close_file()
{
	fclose(file_output);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	write_file
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void write_file(char* data, int length)
--									char* data - the data to write to file
--									int length - length of data to write
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to write data to a file. Should call open_file before using this
--	function
--------------------------------------------------------------------------------------*/
void write_file(char* data, int length)
{
	fwrite(data, length, 1, file_output);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_sending_file
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_sending_file()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to begin the completion routine for sending the requested file to client
--------------------------------------------------------------------------------------*/
void start_sending_file()
{
	memset(buf, 0, FTP_PACKET_SIZE);
	size_t bytes_read = fread(buf, 1, FTP_PACKET_SIZE, requested_file);

	memcpy(FtpSocketInfo->DataBuf.buf, buf, bytes_read);
	FtpSocketInfo->DataBuf.len = (ULONG)bytes_read;

	if (WSASend(FtpSocketInfo->Socket, &(FtpSocketInfo->DataBuf), 1, &SendBytes, 0,
		&(FtpSocketInfo->Overlapped), FTP_SendRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

void send_file_not_found_packet()
{
	memcpy(FtpSocketInfo->DataBuf.buf, file_not_found_packet, 1);
	FtpSocketInfo->DataBuf.len = 1;

	update_server_msgs("Requested file not found");
	if (WSASend(FtpSocketInfo->Socket, &(FtpSocketInfo->DataBuf), 1, &SendBytes, 0,
		&(FtpSocketInfo->Overlapped), NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_receiving_file
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_receiving_file(int type, LPCWSTR request)
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to begin the completion routine for receiving file from server
--------------------------------------------------------------------------------------*/
void start_receiving_file(int type, LPCWSTR request)
{
	DWORD RecvBytes;

	DWORD Flags = 0;

	ZeroMemory(&(FtpSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	FtpSocketInfo->DataBuf.buf = FtpSocketInfo->FTP_BUFFER;
	FtpSocketInfo->DataBuf.len = FTP_PACKET_SIZE;

	Flags = 0;

	if (WSARecv(FtpSocketInfo->Socket, &(FtpSocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(FtpSocketInfo->Overlapped), FTP_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			int temp = WSAGetLastError();
			update_client_msgs("WSARecv() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}

}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	ReceiveFileThreadFunc
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI ReceiveFileThreadFunc(LPVOID lpParameter)
--								LPVOID lpParameter - LPFTP_INFO struct containing the event
--
--	RETURNS:		void
--
--	NOTES:
--	Thread function to initiate and complete the client side file transfer
--------------------------------------------------------------------------------------*/
DWORD WINAPI ReceiveFileThreadFunc(LPVOID lpParameter)
{
	DWORD Index;
	WSAEVENT EventArray[1];
	isReceivingFile = TRUE;
	LPFTP_INFO info = (LPFTP_INFO)lpParameter;
	EventArray[0] = info->FtpCompleteEvent;

	start_receiving_file(WAV_FILE_REQUEST_TYPE, info->filename);
	while (isReceivingFile)
	{
		Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

		if (Index == WSA_WAIT_FAILED)
		{
			update_client_msgs("WSAWaitForMultipleEvents failed with error " + std::to_string(WSAGetLastError()));

			return FALSE;
		}
	}
	close_file();

	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	FTP_ReceiveRoutine
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void CALLBACK FTP_ReceiveRoutine(DWORD Error, DWORD BytesTransferred,
--												LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--										DWORD Error - WSA error
--										DWORD BytesTransferred - number of bytes transferred
--										LPWSAOVERLAPPED Overlapped - overlapped struct
--										DWORD InFlags - unused flags
--
--	RETURNS:		void
--
--	NOTES:
--	Completion routine for receiving file from server and writing to a file.
--	Make sure to have opened the file using open_file before entering this completion routine.
--------------------------------------------------------------------------------------*/
void CALLBACK FTP_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;

	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;
	if (Error != 0)
	{
		update_client_msgs("I/O operation failed with error");
	}

	if (BytesTransferred == 0 || BytesTransferred == 1)
	{
		if (strncmp(SI->DataBuf.buf, file_not_found_packet, 1) == 0)
		{
			//FILE NOT FOUND PROCESS
			finalize_ftp("File Not Found on server.");
		}
		else if (strncmp(SI->DataBuf.buf, ftp_complete_packet, 1) == 0)
		{
			//FTP COMPLETE PROCESS
			finalize_ftp("File transfer completed.");
		}
		update_client_msgs("Closing ftp socket ");

		isReceivingFile = FALSE;
		TriggerWSAEvent(SI->CompletedEvent);
		WSAResetEvent(SI->CompletedEvent);
		return;
	}

	if (BytesTransferred != FTP_PACKET_SIZE)
	{
		if (SI->DataBuf.buf[BytesTransferred] == FILE_NOT_FOUND)
		{
			finalize_ftp("File Not Found on server.");
		}
		else if (SI->DataBuf.buf[BytesTransferred] == TRANSFER_COMPLETE)
		{
			finalize_ftp("File transfer completed.");
		}
	}

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	SI->BytesRECV += BytesTransferred;

	if (SI->BytesRECV == FTP_PACKET_SIZE)
	{
		SI->DataBuf.len = FTP_PACKET_SIZE;
		SI->BytesRECV = 0;
	}
	else if (BytesTransferred > 0)
	{
		SI->DataBuf.len = FTP_PACKET_SIZE - SI->BytesRECV;
	}

	write_file(SI->DataBuf.buf, BytesTransferred);

	SI->DataBuf.buf = SI->FTP_BUFFER;
	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), FTP_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecv() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	FTP_SendRoutine
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred,
--												LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--										DWORD Error - WSA error
--										DWORD BytesTransferred - number of bytes transferred
--										LPWSAOVERLAPPED Overlapped - overlapped struct
--										DWORD InFlags - unused flags
--
--	RETURNS:		void
--
--	NOTES:
--	Completion routine for sending file to client.
--------------------------------------------------------------------------------------*/
void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	size_t bytes_read;
	// Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0)
	{
		update_server_msgs("I/O operation failed with error " + Error);
	}

	if (BytesTransferred == 0)
	{
		update_server_msgs("Closing ftp socket");
	}

	if (Error != 0 || BytesTransferred == 1)
	{
		GlobalFree(SI);
		return;
	}

	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	if (!feof(requested_file))
	{
		memset(buf, 0, strlen(buf));
		bytes_read = fread(buf, 1, FTP_PACKET_SIZE, requested_file);

		memcpy(SI->DataBuf.buf, buf, bytes_read);
		SI->DataBuf.len = (ULONG)bytes_read;

		if (WSASend(SI->Socket, &(SI->DataBuf), 1, &SendBytes, 0,
			&(SI->Overlapped), FTP_SendRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
				return;
			}
		}
	}
	else
	{
		SI->DataBuf.buf = ftp_complete_packet;
		SI->DataBuf.len = 1;
		update_server_msgs("File transfer completed");
		if (WSASend(SI->Socket, &(SI->DataBuf), 1, &SendBytes, 0,
			&(SI->Overlapped), NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
				return;
			}
		}
	}
}

void terminateFtpHandler()
{
	if (isReceivingFile)
	{
		isReceivingFile = FALSE;
		TriggerWSAEvent(FtpSocketInfo->CompletedEvent);
	}
}