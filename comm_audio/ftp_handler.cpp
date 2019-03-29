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

LPSOCKET_INFORMATION SocketInfo;
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

	ftp_complete_packet[0] = FTP_COMPLETE;
	file_not_found_packet[0] = FILE_NOT_FOUND;

	// Create a socket information structure to associate with the accepted socket.
	if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		update_client_msgs("GlobalAlloc failed with error " + GetLastError());
		terminate_connection();
		return;
	}

	// Fill in the details of the server socket to receive file from
	SocketInfo->Socket = *socket;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	SocketInfo->BytesSEND = 0;
	SocketInfo->BytesRECV = 0;
	SocketInfo->DataBuf.len = FTP_PACKET_SIZE;
	SocketInfo->DataBuf.buf = SocketInfo->FTP_BUFFER;
	if (ftpCompletedEvent != NULL)
	{
		SocketInfo->CompletedEvent = ftpCompletedEvent;
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
	int bytes_read = fread(buf, 1, FTP_PACKET_SIZE, requested_file);

	memcpy(SocketInfo->DataBuf.buf, buf, bytes_read);
	SocketInfo->DataBuf.len = bytes_read;

	if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, 0,
		&(SocketInfo->Overlapped), FTP_SendRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("WSASend() failed with error " + WSAGetLastError());
			return;
		}
	}
}

void send_file_not_found_packet()
{
	memcpy(SocketInfo->DataBuf.buf, file_not_found_packet, 1);
	SocketInfo->DataBuf.len = 1;

	update_server_msgs("Requested file not found");
	if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, 0,
		&(SocketInfo->Overlapped), NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("WSASend() failed with error " + WSAGetLastError());
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
	size_t i;
	DWORD RecvBytes;

	DWORD Flags = 0;

	char* req_msg = (char *)malloc(MAX_INPUT_LENGTH);

	wcstombs_s(&i, req_msg, MAX_INPUT_LENGTH, request, MAX_INPUT_LENGTH);

	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	SocketInfo->DataBuf.buf = SocketInfo->FTP_BUFFER;
	SocketInfo->DataBuf.len = FTP_PACKET_SIZE;

	Flags = 0;

	if (WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), FTP_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			int temp = WSAGetLastError();
			update_client_msgs("WSARecv() failed with error " + WSAGetLastError());
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
		while (isReceivingFile)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				update_client_msgs("WSAWaitForMultipleEvents failed with error " + WSAGetLastError());
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				break;
			}

			if (!isReceivingFile) {
				close_file();
			}
		}
	}

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
		update_client_msgs("I/O operation failed with error " + Error);
	}

	if (BytesTransferred == 0 || BytesTransferred == 1)
	{
		if (strncmp(SI->DataBuf.buf, file_not_found_packet, 1) == 0)
		{
			//FILE NOT FOUND PROCESS
			//update_client_msgs("File Not Found on server.");
			finalize_ftp("File Not Found on server.");
		}
		else if (strncmp(SI->DataBuf.buf, ftp_complete_packet, 1) == 0)
		{
			//FTP COMPLETE PROCESS
			//update_client_msgs("File transfer completed.");
			finalize_ftp("File transfer completed.");
		}
		update_client_msgs("Closing ftp socket " + SI->Socket);
		
		isReceivingFile = FALSE;
		TriggerEvent(SI->CompletedEvent);
		return;
	}

	if (BytesTransferred != FTP_PACKET_SIZE) {
		if (SI->DataBuf.buf[BytesTransferred] == FILE_NOT_FOUND) {
			finalize_ftp("File Not Found on server.");
		}
		else if (SI->DataBuf.buf[BytesTransferred] == FTP_COMPLETE) {
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
			update_client_msgs("WSARecv() failed with error " + WSAGetLastError());
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
		update_server_msgs("Closing ftp socket " + SI->Socket);
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
				update_server_msgs("WSASend() failed with error " + WSAGetLastError());
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
				update_server_msgs("WSASend() failed with error " + WSAGetLastError());
				return;
			}
		}
	}
}

