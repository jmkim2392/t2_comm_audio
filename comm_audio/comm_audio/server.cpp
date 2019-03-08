#include "server.h"

SOCKET tcp_socket;
SOCKET udp_socket;

SOCKET RequestSocket;

HANDLE AcceptThread;
HANDLE RequestReceiverThread;
HANDLE RequestHandlerThread;

REQUEST_HANDLER_INFO req_handler_info;

bool isAcceptingConnections = FALSE;
WSAEVENT AcceptEvent;
WSAEVENT RequestReceivedEvent;

DWORD serverThreads[20];
int threadCount = 0;

void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port) {

	DWORD ThreadId;

	//open tcp socket 
	initialize_wsa(tcp_port);
	open_socket(&tcp_socket, SOCK_STREAM,IPPROTO_TCP);

	//open udp socket
	initialize_wsa(udp_port);
	open_socket(&udp_socket, SOCK_DGRAM, IPPROTO_UDP);
	
	initialize_events();

	start_request_receiver();
	start_request_handler();

	if ((AcceptThread = CreateThread(NULL, 0, connection_monitor, (LPVOID)&tcp_socket, 0, &ThreadId)) == NULL)
	{
		printf("Creating Host Thread failed with error %d\n", GetLastError());
		return;
	}
	add_new_thread(ThreadId);

}

void initialize_events()
{
	if ((AcceptEvent = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		printf("WSACreateEvent() failed with error %d\n", WSAGetLastError());
		return;
	}

	if ((RequestReceivedEvent = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		printf("WSACreateEvent() failed with error %d\n", WSAGetLastError());
		return;
	}
}

void start_request_receiver() 
{

	DWORD ThreadId;

	req_handler_info.event = AcceptEvent;
	req_handler_info.req_sock = RequestSocket;
	req_handler_info.CompleteEvent = RequestReceivedEvent;

	if ((RequestReceiverThread = CreateThread(NULL, 0, RequestReceiverThreadFunc, (LPVOID)&req_handler_info, 0, &ThreadId)) == NULL)
	{
		printf("CreateThread failed with error %d\n", GetLastError());
		return;
	}
	add_new_thread(ThreadId);
}

void start_request_handler()
{
	DWORD ThreadId;

	if ((RequestHandlerThread = CreateThread(NULL, 0, HandleRequest, (LPVOID)RequestReceivedEvent, 0, &ThreadId)) == NULL)
	{
		printf("CreateThread failed with error %d\n", GetLastError());
		return;
	}
	add_new_thread(ThreadId);
}

DWORD WINAPI connection_monitor(LPVOID tcp_socket) {

	isAcceptingConnections = TRUE;

	SOCKET* socket = (SOCKET*)tcp_socket;

	if (listen(*socket, 5))
	{
		printf("listen() failed with error %d\n", WSAGetLastError());
		return WSAGetLastError();
	}
	

	while (isAcceptingConnections)
	{
		req_handler_info.req_sock = accept(*socket, NULL, NULL);

		if (WSASetEvent(AcceptEvent) == FALSE)
		{
			printf("WSASetEvent failed with error %d\n", WSAGetLastError());
			return WSAGetLastError();
		}
	}
	return 0;
}

void add_new_thread(DWORD threadId) 
{
	serverThreads[threadCount++] = threadId;
}

void terminate_server()
{
	isAcceptingConnections = FALSE;
}