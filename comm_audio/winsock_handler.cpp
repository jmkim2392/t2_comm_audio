/*-------------------------------------------------------------------------------------
--	SOURCE FILE: winsock_handler.cpp - Contains winsock related functions
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_wsa(LPCWSTR port_number);
--					void open_socket(SOCKET* socket, int socket_type, int protocol_type);
--					void terminate_connection();
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
#include "winsock_handler.h"

WSADATA wsaData;

int s_port;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_wsa
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_wsa(LPCWSTR port_number)
--									LPCWSTR port_number - port number to open
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to initialize wsa with a port number
--------------------------------------------------------------------------------------*/
void initialize_wsa(LPCWSTR port_number, SOCKADDR_IN* InternetAddr)
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

	InternetAddr->sin_family = AF_INET;
	InternetAddr->sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr->sin_port = htons(s_port);

	free(port_num);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	open_socket
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void open_socket(SOCKET* socket, int socket_type, int protocol_type)
--									SOCKET* socket - the socket to open
--									int socket_type - type of socket (SOCK_STREAM/SOCK_DGRAM)
--									int protocol_type - type of protocol (IPPROTO_TCP/IPPROTO_UDP)
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to open a socket with the specified type
--------------------------------------------------------------------------------------*/
void open_socket(SOCKET* socket, int socket_type, int protocol_type)
{
	if ((*socket = WSASocket(AF_INET, socket_type, protocol_type, NULL, 0,
		WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
	{
		printf("Failed to get a socket %d\n", WSAGetLastError());
		terminate_connection();
		return;
	}
}

void close_socket(SOCKET* socket)
{
	closesocket(*socket);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	terminate_connection
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void terminate_connection()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to cleanup wsa
--------------------------------------------------------------------------------------*/
void terminate_connection()
{
	WSACleanup();
	return;
}