#include "astream_handler.h"

// buffer to store file
extern char *buf;
extern LPSOCKET_INFORMATION SocketInfo;
extern FILE* file_output;
extern BOOL isReceivingFile;

// TODO: Solve Conflict with ftp_handler.cpp
extern void close_file();

// TODO: Discuss with Jason those handlers should be in the same file

// headers
void start_receiving_astream(int type, LPCWSTR request);
void initialize_astream(SOCKET* socket, WSAEVENT astreamCompletedEvent);
DWORD WINAPI ReceiveAStreamThreadFunc(LPVOID lpParameter);

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
void initialize_astream(SOCKET* socket, WSAEVENT astreamCompletedEvent)
{
	// UDP init? -> nope. this is for req message -> nope. this is for reading file
	// Seems this init function called by both client mode and server mode
	buf = (char*)malloc(ASTREAM_PACKET_SIZE);

	
	// Create a socket information structure to associate with the accepted socket.
	if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		printf("GlobalAlloc() failed with error %d\n", GetLastError());
		terminate_connection();
		return;
	}

	// Fill in the details of the server socket to receive file from
	SocketInfo->Socket = *socket;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	SocketInfo->BytesSEND = 0;
	SocketInfo->BytesRECV = 0;
	SocketInfo->DataBuf.len = ASTREAM_PACKET_SIZE;
	// TODO: Discuss with Jason whether the SOCKET_INFOMATION structure
	// should be common or dedicated -> possibly there is a need to use
	// different packet size among services (File receive vs Audio stream receive)
	SocketInfo->DataBuf.buf = SocketInfo->FTP_BUFFER;
	if (astreamCompletedEvent != NULL)
	{
		SocketInfo->CompletedEvent = astreamCompletedEvent;
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
void start_receiving_astream(int type, LPCWSTR request) 
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
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
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
DWORD WINAPI ReceiveAStreamThreadFunc(LPVOID lpParameter)
{
	DWORD Index;
	WSAEVENT EventArray[1];
	isReceivingFile = TRUE;
	LPFTP_INFO info = (LPFTP_INFO)lpParameter;
	EventArray[0] = info->FtpCompleteEvent;

	// TODO: keishi replace 
	//start_receiving_file(WAV_FILE_REQUEST_TYPE, info->filename);
	start_receiving_astream(AUDIO_STREAM_REQUEST_TYPE, info->filename);

	while (isReceivingFile)
	{
		while (isReceivingFile)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED) 
			{
				int temp = WSAGetLastError();
				printf("WSAWaitForMultipleEvents failed with error %d\n", WSAGetLastError());
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




