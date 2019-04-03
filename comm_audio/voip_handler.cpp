#include "voip_handler.h"

// NOTES
// - ports have to be opposite for client and server
//		-> client send = server receive
// - both client and server could use these functions
// - receiving: use start_receiving_stream() as it already has a routine and plays audio
//		-> problem: want to specify different socket and port





DWORD WINAPI ReceiverThreadFunc(LPVOID lpParameter)
{
	LPREQUEST_PACKET packet = (LPREQUEST_PACKET)lpParameter;

	SOCKET sock;
	struct sockaddr_in server;
	int data_size;
	struct sockaddr_in client;
	char buf[MAXLEN];

	// Create a datagram socket
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		char debug_buf[512];
		sprintf_s(debug_buf, sizeof(debug_buf), "%d\n", WSAGetLastError());
		OutputDebugStringA(debug_buf);
		exit(1);
	}

	// Bind an address to the socket
	memset((char *)&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(RECEIVING_PORT);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) == -1)		// ERROR 10048: address already in use
	{
		char debug_buf[512];
		sprintf_s(debug_buf, sizeof(debug_buf), "%d\n", WSAGetLastError());
		OutputDebugStringA(debug_buf);
		exit(1);
	}

	// TODO: comment after testing regular data
	while (TRUE)
	{
		int client_len = sizeof(client);
		if ((data_size = recvfrom(sock, buf, MAXLEN, 0, (struct sockaddr *)&client, &client_len)) < 0)
		{
			printf("%d\n", WSAGetLastError());
			exit(1);
		}

		printf("Received %d bytes\t", data_size);
		printf("From host: %s\n", inet_ntoa(client.sin_addr));

		// play audio?
	}

	// TODO: uncomment after testing regular data
	// this will just use the socket in that function
	//start_receiving_stream();

	closesocket(sock);
}

DWORD WINAPI SenderThreadFunc(LPVOID lpParameter)
{
	LPREQUEST_PACKET packet = (LPREQUEST_PACKET)lpParameter;

	SOCKET sock;
	char buf[MAXLEN] = "hello";
	int data_size = 64;
	struct sockaddr_in client;
	HOSTENT *hp;

	// Create a datagram socket
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("Can't create a socket");
		exit(1);
	}

	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(SEND_PORT);

	int client_len = sizeof(client);

	if ((hp = gethostbyname("127.0.0.1")) == NULL)
	{
		fprintf(stderr, "Can't get server's IP address\n");
		exit(1);
	}

	memcpy((char *)&client.sin_addr, hp->h_addr, hp->h_length);

	while (TRUE)
	{
		// record audio?

		if (sendto(sock, buf, data_size, 0, (struct sockaddr *)&client, client_len) != data_size)
		{
			perror("sendto error");
			exit(1);
		}
		printf("Sent data");
	}

	closesocket(sock);
}