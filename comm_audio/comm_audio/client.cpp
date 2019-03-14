#include "client.h"

SOCKET cl_tcp_req_socket;
SOCKET cl_udp_audio_socket;

SOCKADDR_IN cl_addr;
SOCKADDR_IN server_addr;

DWORD clientThreads[20];
int cl_threadCount;

void initialize_client(LPCWSTR tcp_port, LPCWSTR udp_port, LPCWSTR svr_ip_addr)
{
	//open tcp socket 
	initialize_wsa(tcp_port, &cl_addr);
	open_socket(&cl_tcp_req_socket, SOCK_STREAM, IPPROTO_TCP);


	setup_svr_addr(&server_addr, tcp_port, svr_ip_addr);

	if (connect(cl_tcp_req_socket, (struct sockaddr *)&server_addr, sizeof(sockaddr)) == -1)
	{
		fprintf(stderr, "Can't connect to server\n");
		perror("connect");
		exit(1);
	}

	//open udp socket
	initialize_wsa(udp_port, &cl_addr);
	open_socket(&cl_udp_audio_socket, SOCK_DGRAM, IPPROTO_UDP);
}


void setup_svr_addr(SOCKADDR_IN* svr_addr , LPCWSTR tcp_port, LPCWSTR svr_ip_addr)
{
	size_t i;
	int port;
	char* port_num = (char *)malloc(MAX_INPUT_LENGTH);
	char* ip = (char *)malloc(MAX_INPUT_LENGTH);
	struct hostent	*hp;

	wcstombs_s(&i, port_num, MAX_INPUT_LENGTH, tcp_port, MAX_INPUT_LENGTH);

	wcstombs_s(&i, ip, MAX_INPUT_LENGTH, svr_ip_addr, MAX_INPUT_LENGTH);

	port = atoi(port_num);


	// Initialize and set up the address structure
	memset((char *)&svr_addr, 0, sizeof(SOCKADDR_IN));
	svr_addr->sin_family = AF_INET;
	svr_addr->sin_port = htons(port);

	if ((hp = gethostbyname(ip)) == NULL)
	{
		fprintf(stderr, "Unknown server address\n");
		exit(1);
	}

	// Copy the server address
	memcpy((char *)&svr_addr->sin_addr, hp->h_addr, hp->h_length);
}

void send_request()
{

}




void terminate_client()
{

}