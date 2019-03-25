#include "pstream_handler.h"

// buffer to store file
extern char *buf;
extern LPSOCKET_INFORMATION SocketInfo;
extern FILE* file_output;
extern BOOL isReceivingFile;

// TODO: Solve Conflict with ftp_handler.cpp
extern void close_file();

// TODO: Discuss with Jason those handlers should be in the same file


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
void initialize_pstream(SOCKET* socket, WSAEVENT pstreamCompletedEvent)
{
	// UDP init? -> nope. this is for req message -> nope. this is for reading file
	// Seems this init function called by both client mode and server mode
	buf = (char*)malloc(PSTREAM_PACKET_SIZE);

	
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
	SocketInfo->DataBuf.len = PSTREAM_PACKET_SIZE;
	// TODO: Discuss with Jason whether the SOCKET_INFOMATION structure
	// should be common or dedicated -> possibly there is a need to use
	// different packet size among services (File receive vs Audio stream receive)
	SocketInfo->DataBuf.buf = SocketInfo->FTP_BUFFER;
	if (pstreamCompletedEvent != NULL)
	{
		SocketInfo->CompletedEvent = pstreamCompletedEvent;
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
DWORD WINAPI ReceivePStreamThreadFunc(LPVOID lpParameter)
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




