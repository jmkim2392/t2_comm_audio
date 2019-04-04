/*-------------------------------------------------------------------------------------
--	SOURCE FILE: file_streaming_handler.cpp - Contains file stream functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_file_stream(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT fileStreamCompletedEvent, HANDLE eventTrigger);
--					int open_file_to_stream(std::string filename);
--					void start_sending_file_stream();
--					void send_file_not_found_packet_udp();
--					void close_file_streaming();
--					void start_receiving_stream();
--					DWORD WINAPI ReceiveStreamThreadFunc(LPVOID lpParameter);
--					void CALLBACK FileStream_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					void CALLBACK FileStream_SendRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					DWORD WINAPI SendStreamThreadFunc(LPVOID lpParameter);
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--
--------------------------------------------------------------------------------------*/
#include "file_streaming_handler.h"

using namespace std;

FILE* requested_file_stream;
DWORD SendBytes_FileStream;

LPSOCKET_INFORMATION FileStreamSocketInfo;
BOOL isReceivingFileStream = FALSE;

SOCKADDR_IN client;
int client_len = sizeof(sockaddr_in);

char fileStream_complete[AUDIO_PACKET_SIZE];
char fileStream_not_found[AUDIO_PACKET_SIZE];
char *file_stream_buf;

int num_packet = 0;
int total_packet = 0;
int packetNUmRecv = 0;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_file_stream
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_file_stream(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT fileStreamCompletedEvent, HANDLE eventTrigger)
--									SOCKET* socket - the type of request
--									SOCKADDR_IN* addr message - the request message
--									WSAEVENT fileStreamCompletedEvent - WSAEvent completed to use as trigger
--									HANDLE eventTrigger - resumeSend Event trigger
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to begin the file streaming process
--------------------------------------------------------------------------------------*/
void initialize_file_stream(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT fileStreamCompletedEvent, HANDLE eventTrigger)
{
	file_stream_buf = (char*)malloc(AUDIO_PACKET_SIZE);
	strcpy_s(fileStream_complete, 18,"Transfer Complete");

	// Create a socket information structure
	if ((FileStreamSocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		//terminate_connection();
		return;
	}

	// Fill in the details of the server socket to receive file from
	FileStreamSocketInfo->Socket = *socket;
	ZeroMemory(&(FileStreamSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	FileStreamSocketInfo->BytesSEND = 0;
	FileStreamSocketInfo->BytesRECV = 0;
	if (addr != NULL) 
	{
		FileStreamSocketInfo->Sock_addr = *addr;
	}
	FileStreamSocketInfo->DataBuf.len = AUDIO_PACKET_SIZE;
	FileStreamSocketInfo->DataBuf.buf = FileStreamSocketInfo->AUDIO_BUFFER;
	if (fileStreamCompletedEvent != NULL)
	{
		FileStreamSocketInfo->CompletedEvent = fileStreamCompletedEvent;
	}
	if (eventTrigger != NULL)
	{
		FileStreamSocketInfo->EventTrigger = eventTrigger;
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	open_file_to_stream
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		int open_file_to_stream(std::string filename)
--									std::string filename - name of the file to open
--
--	RETURNS:		int - 0 for success, else file not found
--
--	NOTES:
--	Call this function to open a file for streaming process
--------------------------------------------------------------------------------------*/
int open_file_to_stream(std::string filename)
{
	return fopen_s(&requested_file_stream, filename.c_str(), "rb");
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	close_file_streaming
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void close_file_streaming()
--
--	RETURNS:		int - 0 for success, else file not found
--
--	NOTES:
--	TODO: add proper closing of file after streaming finished
--------------------------------------------------------------------------------------*/
void close_file_streaming() {
	fclose(requested_file_stream);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	SendStreamThreadFunc
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI SendStreamThreadFunc(LPVOID lpParameter)
--										LPVOID lpParameter - the event to signify end of file stream
--
--	RETURNS:		DWORD - 0 Thread terminated
--
--	NOTES:
--	Thread function for Sending file stream to client
--------------------------------------------------------------------------------------*/
DWORD WINAPI SendStreamThreadFunc(LPVOID lpParameter)
{
	DWORD Index;
	WSAEVENT EventArray[1];
	isReceivingFileStream = TRUE;
	EventArray[0] = (WSAEVENT)lpParameter;
	
	memset(file_stream_buf, 0, AUDIO_PACKET_SIZE);
	int bytes_read = fread(file_stream_buf, 1, AUDIO_PACKET_SIZE, requested_file_stream);

	memcpy(FileStreamSocketInfo->DataBuf.buf, file_stream_buf, bytes_read);
	FileStreamSocketInfo->DataBuf.len = bytes_read;
	num_packet++;
	total_packet++;

	if (WSASendTo(FileStreamSocketInfo->Socket, &(FileStreamSocketInfo->DataBuf), 1, &SendBytes_FileStream, 0, (SOCKADDR *)&(FileStreamSocketInfo->Sock_addr), sizeof(FileStreamSocketInfo->Sock_addr), &(FileStreamSocketInfo->Overlapped), FileStream_SendRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("Send UDP Packet failed with error " + std::to_string(WSAGetLastError()));
			return 0;
		}
	}
	while (isReceivingFileStream)
	{
		Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

		if (Index == WSA_WAIT_FAILED)
		{
			update_client_msgs("WSAWaitForMultipleEvents failed with error " + std::to_string(WSAGetLastError()));
			terminate_connection();
			return FALSE;
		}

		if (Index != WAIT_IO_COMPLETION)
		{
			break;
		}

		if (!isReceivingFileStream) {
			close_file_streaming();
		}
	}

	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	send_file_not_found_packet_udp
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void send_file_not_found_packet_udp() 
--
--	RETURNS:		void
--
--	NOTES:
--	Call this thread to send a file not found udp packet to the client
--	TODO
--------------------------------------------------------------------------------------*/
void send_file_not_found_packet_udp() {

}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	ReceiveStreamThreadFunc
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI SendStreamThreadFunc(LPVOID lpParameter)
--										LPVOID lpParameter - the event to signify end of file stream
--
--	RETURNS:		DWORD - 0 Thread terminated
--
--	NOTES:
--	Thread function for Receiving file stream from server
--------------------------------------------------------------------------------------*/
DWORD WINAPI ReceiveStreamThreadFunc(LPVOID lpParameter)
{
	DWORD Index;
	WSAEVENT EventArray[1];
	isReceivingFileStream = TRUE;
	EventArray[0] = (WSAEVENT) lpParameter;
	start_receiving_stream();

	while (isReceivingFileStream)
	{
		while (isReceivingFileStream)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				update_client_msgs("WSAWaitForMultipleEvents failed with error " + std::to_string(WSAGetLastError()));
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				break;
			}

			if (!isReceivingFileStream) {
				close_file_streaming();
			}
		}
	}

	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_receiving_stream
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_receiving_stream()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this thread to start receiving the file stream from Server
--------------------------------------------------------------------------------------*/
void start_receiving_stream()
{
	size_t i;
	DWORD RecvBytes;
	DWORD Flags = 0;

	int iResult = 0;
	int iOptVal = 0;
	int iOptLen = sizeof(int);

	ZeroMemory(&(FileStreamSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	FileStreamSocketInfo->DataBuf.buf = FileStreamSocketInfo->AUDIO_BUFFER;
	FileStreamSocketInfo->DataBuf.len = AUDIO_PACKET_SIZE;

	Flags = 0;

	if (WSARecvFrom(FileStreamSocketInfo->Socket, &(FileStreamSocketInfo->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& client, &client_len, &(FileStreamSocketInfo->Overlapped), FileStream_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecvFrom() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	FileStream_ReceiveRoutine
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void CALLBACK FileStream_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--
--	RETURNS:		void
--
--	NOTES:
--	Completion routine for receiving file stream from server
--------------------------------------------------------------------------------------*/
void CALLBACK FileStream_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;

	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;
	if (Error != 0)
	{
		update_client_msgs("I/O operation failed with error " + std::to_string(Error));
	}

	packetNUmRecv++;
	update_client_msgs("PacketRecv: " + std::to_string(packetNUmRecv));

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	writeToAudioBuffer(SI->DataBuf.buf);

	SI->DataBuf.buf = SI->AUDIO_BUFFER;
	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& client, &client_len, &(SI->Overlapped), FileStream_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecv() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	FileStream_SendRoutine
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void CALLBACK FileStream_SendRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--
--	RETURNS:		void
--
--	NOTES:
--	Completion routine for sending file stream to client
--------------------------------------------------------------------------------------*/
void CALLBACK FileStream_SendRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	size_t bytes_read;
	// Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;
	
	num_packet++;
	total_packet++;
	update_server_msgs("Total Packets Sent: " + std::to_string(total_packet));

	if (Error != 0)
	{
		update_server_msgs("I/O operation failed with error " + std::to_string(Error));
	}

	if (BytesTransferred == 0)
	{
		update_server_msgs("Closing ftp socket " + std::to_string(SI->Socket));
	}

	if (Error != 0 || BytesTransferred == 1)
	{
		GlobalFree(SI);
		return;
	}

	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	if (num_packet == MAX_NUM_STREAM_PACKETS) {
		WaitForSingleObject(SI->EventTrigger ,INFINITE);
		ResetEvent(SI->EventTrigger);
		num_packet = 0;
	}

	if (!feof(requested_file_stream))
	{
		memset(file_stream_buf, 0, strlen(file_stream_buf));
		bytes_read = fread(file_stream_buf, 1, AUDIO_PACKET_SIZE, requested_file_stream);

		memcpy(SI->DataBuf.buf, file_stream_buf, bytes_read);
		SI->DataBuf.len = (ULONG)bytes_read;

		if (WSASendTo(SI->Socket, &(SI->DataBuf), 1, &SendBytes_FileStream, 0, (SOCKADDR *)&(SI->Sock_addr), sizeof(SI->Sock_addr), &(SI->Overlapped), FileStream_SendRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
				return;
			}
		}
	}
	return;
}

