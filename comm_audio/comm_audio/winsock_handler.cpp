#include "winsock_handler.h"

WSADATA wsaData;

SOCKADDR_IN InternetAddr;
int s_port;

bool isAcceptingConnections = FALSE;

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

void start_connection_monitor(SOCKET* tcp_socket) {

	isAcceptingConnections = TRUE;

	if (listen(*tcp_socket, 5))
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
		HostSocket = accept(*tcp_socket, NULL, NULL);

		if (WSASetEvent(AcceptEvent) == FALSE)
		{
			printf("WSASetEvent failed with error %d\n", WSAGetLastError());
		}
	}
}

void terminate_connection()
{
	WSACleanup();
	return;
}