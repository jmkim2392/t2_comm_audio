/*-------------------------------------------------------------------------------------
--	SOURCE FILE: request_handler.cpp - Contains request related functions
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter);
--					void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred, 
												LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					DWORD WINAPI HandleRequest(LPVOID lpParameter);
--					void TriggerEvent(WSAEVENT event);
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--
--------------------------------------------------------------------------------------*/
#include "request_handler.h"

bool isAcceptingRequests = FALSE;
circular_buffer<std::string> request_buffer(10);

/*-------------------------------------------------------------------------------------
--	FUNCTION:	RequestReceiverThreadFunc
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter)
--									LPVOID lpParameter - request handler info struct
--
--	RETURNS:		DWORD
--
--	NOTES:
--	Request Receiver Thread function to wait for Accept Events and start receiving requests
--	from client
--------------------------------------------------------------------------------------*/
DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter)
{
	LPSOCKET_INFORMATION SocketInfo;
	LPREQUEST_HANDLER_INFO req_handler_info;
	WSAEVENT EventArray[1];
	DWORD Index;
	DWORD RecvBytes;
	int retVal;
	DWORD BytesSent = 0;
	DWORD Flags = 0;

	req_handler_info = (LPREQUEST_HANDLER_INFO)lpParameter;

	// Save the accept event in the event array.
	EventArray[0] = req_handler_info->event;
	isAcceptingRequests = TRUE;

	while (isAcceptingRequests)
	{
		// Wait for accept() to signal an event and also process WorkerRoutine() returns.
		while (isAcceptingRequests)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				printf("WSAWaitForMultipleEvents failed with error %d\n", WSAGetLastError());
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				// An accept() call event is ready - break the wait loop
				break;
			}
		}

		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

		// Create a socket information structure to associate with the accepted socket.
		if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
			sizeof(SOCKET_INFORMATION))) == NULL)
		{
			printf("GlobalAlloc() failed with error %d\n", GetLastError());
			terminate_connection();
			return FALSE;
		}

		// Fill in the details of our accepted socket.
		SocketInfo->Socket = req_handler_info->req_sock;
		ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
		SocketInfo->BytesSEND = 0;
		SocketInfo->BytesRECV = 0;
		SocketInfo->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
		SocketInfo->DataBuf.buf = SocketInfo->Buffer;
		SocketInfo->CompletedEvent = req_handler_info->CompleteEvent;

		Flags = 0;

		retVal = WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), RequestReceiverRoutine);

		if (retVal == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				wprintf(L"WSARecvFrom failed with error: %ld\n", WSAGetLastError());
				terminate_connection();

				return FALSE;
			}
		}
	}
	return TRUE;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	RequestReceiverRoutine
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred,
--												LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--										DWORD Error - WSA error
--										DWORD BytesTransferred - number of bytes transferred
--										LPWSAOVERLAPPED Overlapped - overlapped struct
--										DWORD InFlags - unused flags
--
--	RETURNS:		void
--
--	NOTES:
--	Completion Routine Callback function to receive request packets
--------------------------------------------------------------------------------------*/
void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;
	std::string packet;

	// Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0)
	{
		printf("I/O operation failed with error %d\n", Error);
	}

	if (BytesTransferred == 0)
	{
		printf("Closing socket %d\n", SI->Socket);
	}

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	if (BytesTransferred == DEFAULT_REQUEST_PACKET_SIZE)
	{
		request_buffer.put(SI->DataBuf.buf);
 		SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
		TriggerEvent(SI->CompletedEvent);
	}
	else if (BytesTransferred > 0) 
	{
		packet = request_buffer.peek();
		if (packet.length() < DEFAULT_REQUEST_PACKET_SIZE) 
		{
			packet += SI->DataBuf.buf;
			request_buffer.update(packet);
			SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE - BytesTransferred;
		} 
		
		if (packet.length() == DEFAULT_REQUEST_PACKET_SIZE)
		{
			//full packet  
			SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
			TriggerEvent(SI->CompletedEvent);
		}
	}

	SI->DataBuf.buf = SI->Buffer;
	
	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), RequestReceiverRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	HandleRequest
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI HandleRequest(LPVOID lpParameter)
--									LPVOID lpParameter - the Packet Received Event
--
--	RETURNS:		void
--
--	NOTES:
--	Thread function to read from circular buffer and handle requests as received
--------------------------------------------------------------------------------------*/
DWORD WINAPI HandleRequest(LPVOID lpParameter)
{
	std::string request;
	DWORD Index;
	WSAEVENT EventArray[1];
	int temp;
	
	isAcceptingRequests = TRUE;
	EventArray[0] = (WSAEVENT) lpParameter;

	while (isAcceptingRequests)
	{
		Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

		temp = request_buffer.peek().length();
		request = request_buffer.get();
		temp = request.length();
		
		////	check if ring buffer is empty, read, and handle accordingly 
		//if (!request_buffer.empty() && request_buffer.peek().length() == DEFAULT_REQUEST_PACKET_SIZE)
		//{
		//	

		//	//parse packet and handle accordingly
		//	/*if (request.compare("REQUEST HERE") == 0) {

		//	}*/

		//} 
		//else
		//{
		//	// request corrupt/wrong format
		//}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	TriggerEvent
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void TriggerEvent(WSAEVENT event) 
--								WSAEVENT event - event to trigger
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize the program as a server
--------------------------------------------------------------------------------------*/
void TriggerEvent(WSAEVENT event) 
{
	if (WSASetEvent(event) == FALSE)
	{
		printf("WSASetEvent failed with error %d\n", WSAGetLastError());
		return;
	}
}