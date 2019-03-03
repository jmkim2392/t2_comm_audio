#include "winsock_handler.h"

WSADATA wsaData;

SOCKADDR_IN InternetAddr;
int s_port;

bool isAcceptingConnections = FALSE;
SOCKET AcceptedSocket;
WSAEVENT AcceptEvent;

void initialize_wsa(LPCWSTR protocol, LPCWSTR port_number)
{
	size_t i;
	int ret_val;
	char* port_num = (char *)malloc(MAX_INPUT_LENGTH);

	if ((ret_val = WSAStartup(wVersionRequested, &wsaData)) != 0)
	{
		printf("WSAStartup failed with error %d\n", ret_val);
		terminate_connection();
		return;
	}

	wcstombs_s(&i, port_num, MAX_INPUT_LENGTH, port_number, MAX_INPUT_LENGTH);
	s_port = atoi(port_num);

	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(s_port);

	free(port_num);
}

void open_socket(SOCKET* tcp_socket, int socket_type, int protocol_type) {
	
	if ((*tcp_socket = WSASocket(AF_INET, socket_type, protocol_type, NULL, 0,
		WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
	{
		printf("Failed to get a socket %d\n", WSAGetLastError());
		terminate_connection();
		return;
	}

	if (bind(*tcp_socket, (PSOCKADDR)&InternetAddr,
		sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		printf("bind() failed with error %d\n", WSAGetLastError());
		terminate_connection();
		return ;
	}
}

DWORD WINAPI  start_connection_monitor(LPVOID tcp_socket) {
	
	SOCKET* socket = (SOCKET*) tcp_socket;

	isAcceptingConnections = TRUE;

	if (listen(*socket, 5))
	{
		printf("listen() failed with error %d\n", WSAGetLastError());
		return;
	}
	if ((AcceptEvent = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		printf("WSACreateEvent() failed with error %d\n", WSAGetLastError());
		return;
	}

	while (isAcceptingConnections)
	{
		AcceptedSocket = accept(*socket, NULL, NULL);

		if (WSASetEvent(AcceptEvent) == FALSE)
		{
			printf("WSASetEvent failed with error %d\n", WSAGetLastError());
		}
	}
}

DWORD WINAPI start_request_handler(LPVOID acceptEvent)
{
	DWORD Flags;
	LPSOCKET_INFORMATION SocketInfo;
	DWORD ret_val;
	DWORD RecvBytes;
	WSAEVENT EventArray[1];
	EventArray[0] = (WSAEVENT)acceptEvent;

	while (TRUE)
	{
		while (TRUE)
		{
			ret_val = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
			switch (ret_val)
			{
			case WSA_WAIT_FAILED :
				break;
			case WAIT_IO_COMPLETION :
				break;
			}
		}
		WSAResetEvent(EventArray[ret_val - WSA_WAIT_EVENT_0]);

		// Create a socket information structure to associate with the accepted socket.
		if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
			sizeof(SOCKET_INFORMATION))) == NULL)
		{
			printf("GlobalAlloc() failed with error %d\n", GetLastError());
			return FALSE;
		}

		// Fill in the details of our accepted socket.
		SocketInfo->Socket = AcceptedSocket;
		ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
		SocketInfo->BytesSEND = 0;
		SocketInfo->BytesRECV = 0;
		SocketInfo->DataBuf.len = DATA_BUFSIZE;
		SocketInfo->DataBuf.buf = SocketInfo->Buffer;

		Flags = 0;

		if (WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags,
			&(SocketInfo->Overlapped), HostWorkerRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				int temp = WSAGetLastError();
				printf("WSARecv() failed with error %d\n", WSAGetLastError());
				return FALSE;
			}
		}
		printf("Socket %d connected\n", AcceptedSocket);

		
	}
}

void terminate_connection()
{
	WSACleanup();
	return;
}