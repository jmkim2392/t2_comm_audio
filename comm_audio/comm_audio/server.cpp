/*-------------------------------------------------------------------------------------
--	SOURCE FILE: server.cpp - Contains server functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port)
--					void start_request_receiver()
--					void start_request_handler()
--					void initialize_events()
--					DWORD WINAPI connection_monitor(LPVOID tcp_socket)
--					void add_new_thread(DWORD threadId)
--					void terminate_server()
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
#include "server.h"

SOCKET tcp_accept_socket;
SOCKET udp_socket;

SOCKET RequestSocket;

HANDLE AcceptThread;
HANDLE RequestReceiverThread;
HANDLE RequestHandlerThread;
HANDLE BroadCastThread;

REQUEST_HANDLER_INFO req_handler_info;
BROADCAST_INFO broadcast_info;

SOCKADDR_IN InternetAddr;

bool isAcceptingConnections = FALSE;
BOOL isBroadcasting = FALSE;
WSAEVENT AcceptEvent;
WSAEVENT RequestReceivedEvent;

DWORD serverThreads[20];
int threadCount = 0;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_server
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port)
--									LPCWSTR tcp_port - port number for tcp
--									LPCWSTR udp_port - port number for udp
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize the program as a server
--------------------------------------------------------------------------------------*/
void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port) 
{
	DWORD ThreadId;

	//open tcp socket 
	initialize_wsa(tcp_port, &InternetAddr);
	open_socket(&tcp_accept_socket, SOCK_STREAM,IPPROTO_TCP);

	if (bind(tcp_accept_socket, (PSOCKADDR)&InternetAddr,
		sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		printf("bind() failed with error %d\n", WSAGetLastError());
		terminate_connection();
		return;
	}

	//open udp socket
	initialize_wsa(udp_port, &InternetAddr);
	open_socket(&udp_socket, SOCK_DGRAM, IPPROTO_UDP);
	
	initialize_events();

	start_request_receiver();
	start_request_handler();
	start_broadcast(&udp_socket, udp_port);

	if ((AcceptThread = CreateThread(NULL, 0, connection_monitor, (LPVOID)&tcp_accept_socket, 0, &ThreadId)) == NULL)
	{
		printf("Creating Host Thread failed with error %d\n", GetLastError());
		return;
	}
	add_new_thread(ThreadId);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_events
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_events()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize the events used by server
--------------------------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_request_receiver
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_request_receiver() 
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start the request receiver which receives request packets from
--	clients
--------------------------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_request_handler
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_request_handler()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start the request handler thread which read and handle the packets
--	received from client
--------------------------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_broadcast
--
--	DATE:			March 11, 2019
--
--	REVISIONS:		March 11, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_broadcast(SOCKET* socket, LPCWSTR udp_port)
--								SOCKET* socket - the udp Socket to multicast
--								LPCWSTR udp_port - udp port number
--	RETURNS:		DWORD
--
--	NOTES:
--	Call this function to initialize and start multicasting audio to clients
--------------------------------------------------------------------------------------*/
void start_broadcast(SOCKET* socket, LPCWSTR udp_port)
{
	DWORD ThreadId;
	size_t i;
	int portNum;
	char* port_num = (char *)malloc(MAX_INPUT_LENGTH);

	isBroadcasting = TRUE;

	wcstombs_s(&i, port_num, MAX_INPUT_LENGTH, udp_port, MAX_INPUT_LENGTH);
	portNum = atoi(port_num);

	broadcast_info.portNum = portNum;
	broadcast_info.udpSocket = *socket;

	if ((BroadCastThread = CreateThread(NULL, 0, broadcast_audio, (LPVOID)&broadcast_info, 0, &ThreadId)) == NULL)
	{
		printf("CreateThread failed with error %d\n", GetLastError());
		return;
	}
	add_new_thread(ThreadId);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	connection_monitor
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI connection_monitor(LPVOID tcp_socket) 
--								LPVOID tcp_socket - the tcp_socket to open and listen
--	RETURNS:		DWORD
--
--	NOTES:
--	Thread function to listen for connection requests and trigger an Accept Event to
--	process connecting clients
--------------------------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	broadcast_audio
--
--	DATE:			March 11, 2019
--
--	REVISIONS:		March 11, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI broadcast_audio(LPVOID broadcastInfo)
--									LPVOID broadcastInfo - struct for multicasting
--
--	RETURNS:		DWORD
--
--	NOTES:
--	Thread function for multicasting an audio stream to clients.
	//TODO: Current implementation is for testing only and uses Broadcast.
	// Will be reimplemented with Multicast as required by Phat
--------------------------------------------------------------------------------------*/
DWORD WINAPI broadcast_audio(LPVOID broadcastInfo)
{
	int retVal;
	BOOL bOpt;
	SOCKADDR_IN   to;
	LPBROADCAST_INFO broadcast_info;
	WSABUF DataBuf;
	WSAOVERLAPPED Overlapped;
	DWORD BytesSent = 0;
	DWORD Flags = 0;

	std::string tempMsg = "Testing Broadcast";

	broadcast_info = (LPBROADCAST_INFO)broadcastInfo;

	bOpt = TRUE;
	isBroadcasting = TRUE;

	retVal = setsockopt(broadcast_info->udpSocket, SOL_SOCKET, SO_BROADCAST, (char *)&bOpt, sizeof(bOpt));
	//TODO: temp for testing to be removed
	//retVal = setsockopt(broadcast_info->udpSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOpt, sizeof(bOpt));
	if (retVal == SOCKET_ERROR)
	{
		printf("setsockopt(SO_BROADCAST) failed: %d\n",
			WSAGetLastError());
		return -1;
	}
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	to.sin_port = htons(broadcast_info->portNum);

	// Make sure the Overlapped struct is zeroed out
	SecureZeroMemory((PVOID)&Overlapped, sizeof(WSAOVERLAPPED));

	// Create an event handle and setup the overlapped structure.
	Overlapped.hEvent = WSACreateEvent();
	if (Overlapped.hEvent == WSA_INVALID_EVENT) {
		printf("WSACreateEvent failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	// while flag on and no error
	while (isBroadcasting) 
	{
		DataBuf.len = 1024;
		DataBuf.buf = &tempMsg[0u];
		retVal = WSASendTo(broadcast_info->udpSocket, &DataBuf, 1, &BytesSent, Flags,(SOCKADDR *)&to, sizeof(to), &Overlapped, NULL);
		if (retVal == SOCKET_ERROR)
		{
			printf("sendto() failed: %d\n", WSAGetLastError());
			break;
		}
		Sleep(10000);
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	add_new_thread
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void add_new_thread(DWORD threadId) 
--								DWORD threadId - the threadId to add
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to add a new thread to maintain the list of active threads
--------------------------------------------------------------------------------------*/
void add_new_thread(DWORD threadId) 
{
	serverThreads[threadCount++] = threadId;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	terminate_server
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void terminate_server()
--
--	RETURNS:		void
--
--	NOTES:
--	TODO implement the rest of server cleanup functions to safely terminate program
--------------------------------------------------------------------------------------*/
void terminate_server()
{
	isAcceptingConnections = FALSE;
}