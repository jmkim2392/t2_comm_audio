#include "request_handler.h"

circular_buffer<std::string> request_buffer(10);

DWORD WINAPI RequestHandlerWorkerThread(LPVOID lpParameter)
{
	LPSOCKET_INFORMATION SocketInfo;
	WSAEVENT EventArray[1];
	DWORD Index;
	DWORD RecvBytes;
	int retVal;
	DWORD BytesSent = 0;
	DWORD Flags = 0;

	// Save the accept event in the event array.
	EventArray[0] = (WSAEVENT)lpParameter;

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
		SocketInfo->Socket = RequestSocket;
		ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
		SocketInfo->BytesSEND = 0;
		SocketInfo->BytesRECV = 0;
		SocketInfo->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
		SocketInfo->DataBuf.buf = SocketInfo->Buffer;

		Flags = 0;

		retVal = WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), RequestHandlerRoutine);

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

void CALLBACK RequestHandlerRoutine(DWORD Error, DWORD BytesTransferred,
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
		//write_to_file(SI->DataBuf.buf, SI->DataBuf.len);
		request_buffer.put(SI->DataBuf.buf);
		SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
	}
	else if (BytesTransferred > 0) 
	{
		packet = request_buffer.peek();
		if (packet.length() <= DEFAULT_REQUEST_PACKET_SIZE) {
			packet += SI->DataBuf.buf;
			request_buffer.update(packet);
		}
		SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE-BytesTransferred;
	}

	SI->DataBuf.buf = SI->Buffer;

	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), RequestHandlerRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			return;
		}
	}
}