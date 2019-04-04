#include "voip_handler.h"

// NOTES
// - ports have to be opposite for client and server
//		-> client send = server receive
// - both client and server could use these functions
// - receiving: use start_receiving_stream() as it already has a routine and plays audio
//		-> problem: want to specify different socket and port


struct sockaddr_in receiving_client;
int receiving_client_len;


DWORD WINAPI ReceiverThreadFunc(LPVOID lpParameter)
{
	//SOCKET sock;
	//struct sockaddr_in server;
	//int data_size;
	//struct sockaddr_in client;
	//char buf[MAXLEN];

	//// Create a datagram socket
	//if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	//{
	//	char debug_buf[512];
	//	sprintf_s(debug_buf, sizeof(debug_buf), "%d\n", WSAGetLastError());
	//	OutputDebugStringA(debug_buf);
	//	exit(1);
	//}

	//// Bind an address to the socket
	//memset((char *)&server, 0, sizeof(server));
	//server.sin_family = AF_INET;
	//server.sin_port = htons(RECEIVING_PORT);
	//server.sin_addr.s_addr = htonl(INADDR_ANY);
	//if (bind(sock, (struct sockaddr *)&server, sizeof(server)) == -1)		// ERROR 10048: address already in use
	//{
		/*char debug_buf[512];
		sprintf_s(debug_buf, sizeof(debug_buf), "%d\n", WSAGetLastError());
		OutputDebugStringA(debug_buf);*/
	//	exit(1);
	//}

	//// TODO: comment after testing regular data
	//while (TRUE)
	//{
	//	int client_len = sizeof(client);
	//	if ((data_size = recvfrom(sock, buf, MAXLEN, 0, (struct sockaddr *)&client, &client_len)) < 0)
	//	{
	//		printf("%d\n", WSAGetLastError());
	//		exit(1);
	//	}

	//	printf("Received %d bytes\t", data_size);
	//	printf("From host: %s\n", inet_ntoa(client.sin_addr));

	//	// play audio?
	//}
	//closesocket(sock);

	DWORD Index;
	WSAEVENT EventArray[1];
	BOOL isReceivingFileStream = TRUE;
	EventArray[0] = (WSAEVENT)lpParameter;

	start_receiving_voip();

	while (isReceivingFileStream)
	{
		while (isReceivingFileStream)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				update_client_msgs("WSAWaitForMultipleEvents failed with error " + std::to_string(WSAGetLastError()));
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				break;
			}
		}
	}
	return 0;
}

void start_receiving_voip()
{
	DWORD RecvBytes;
	DWORD Flags = 0;
	SOCKET receiving_voip_socket;
	SOCKADDR_IN server_addr_udp;
	LPCWSTR udp_port = L"4981";
	receiving_client_len = sizeof(sockaddr_in);

	BOOL isConnected = TRUE;
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	LPSOCKET_INFORMATION VoipSocketInfo;

	//open udp socket
	initialize_wsa(udp_port, &receiving_client);
	open_socket(&receiving_voip_socket, SOCK_DGRAM, IPPROTO_UDP);

	setup_svr_addr(&server_addr_udp, udp_port, L"142.232.48.31");

	//bind to server address
	if (bind(receiving_voip_socket, (struct sockaddr *)&server_addr_udp, sizeof(sockaddr)) == SOCKET_ERROR) {
		isConnected = FALSE;

		update_status(disconnectedMsg);
		update_client_msgs("Failed to bind udp socket " + std::to_string(WSAGetLastError()));
	}

	//set REUSEADDR for udp socket
	if (setsockopt(receiving_voip_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}

	if (isConnected) {
		update_client_msgs("Connected to voip");
		update_status(connectedMsg);
	}

	// TODO: set socket, port, etc
	/*typedef struct _SOCKET_INFORMATION {
		OVERLAPPED Overlapped;
		SOCKET Socket;
		CHAR Buffer[DEFAULT_REQUEST_PACKET_SIZE];
		CHAR FTP_BUFFER[FTP_PACKET_SIZE];
		CHAR AUDIO_BUFFER[AUDIO_BLOCK_SIZE];
		WSABUF DataBuf;
		WSAEVENT CompletedEvent;
		HANDLE EventTrigger;
		SOCKADDR_IN Sock_addr;
		DWORD BytesSEND;
		DWORD BytesRECV;
	} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;*/
	// Create a socket information structure
	if ((VoipSocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		//terminate_connection();
		return;
	}
	ZeroMemory(&(VoipSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	VoipSocketInfo->Socket = receiving_voip_socket;
	VoipSocketInfo->DataBuf.buf = VoipSocketInfo->AUDIO_BUFFER;
	VoipSocketInfo->DataBuf.len = AUDIO_PACKET_SIZE;
	//VoipSocketInfo->Sock_addr = server_addr_udp;

	if (WSARecvFrom(VoipSocketInfo->Socket, &(VoipSocketInfo->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& receiving_client, &receiving_client_len, &(VoipSocketInfo->Overlapped), Voip_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecvFrom() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

void CALLBACK Voip_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	// dasha - not hitting the receive routine...
	DWORD RecvBytes;
	DWORD Flags;

	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;
	if (Error != 0)
	{
		update_client_msgs("I/O operation failed with error " + std::to_string(Error));
	}

	//TODO: Implement end of voip process here

	//if (BytesTransferred == 0 || BytesTransferred == 1)
	//{
	//	//if (strncmp(SI->DataBuf.buf, file_not_found_packet, 1) == 0)
	//	//{
	//	//	//FILE NOT FOUND PROCESS
	//	//	//update_client_msgs("File Not Found on server.");
	//	//	finalize_ftp("File Not Found on server.");
	//	//}
	//	//else if (strncmp(SI->DataBuf.buf, ftp_complete_packet, 1) == 0)
	//	//{
	//	//	//FTP COMPLETE PROCESS
	//	//	//update_client_msgs("File transfer completed.");
	//	//	finalize_ftp("File transfer completed.");
	//	//}
	//	update_client_msgs("Closing ftp socket " + std::to_string(SI->Socket));

	//	isReceivingFileStream = FALSE;
	//	TriggerWSAEvent(SI->CompletedEvent);
	//	return;
	//}

	if (BytesTransferred != AUDIO_PACKET_SIZE) {
		return;
	}

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	
	char debug_buf[512];
	sprintf_s(debug_buf, sizeof(debug_buf), "%s\n", SI->DataBuf.buf);
	OutputDebugStringA(debug_buf);

	OutputDebugStringA("got something");

	//writeToAudioBuffer(SI->DataBuf.buf);

	SI->DataBuf.buf = SI->AUDIO_BUFFER;
	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& receiving_client, &receiving_client_len, &(SI->Overlapped), Voip_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecv() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

DWORD WINAPI SenderThreadFunc(LPVOID lpParameter)
{
	LPREQUEST_PACKET packet = (LPREQUEST_PACKET)lpParameter;

	SOCKET sock;
	char buf[8192] = "hello";
	int data_size = 8192;
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
	client.sin_port = htons(RECEIVING_PORT);

	int client_len = sizeof(client);

	if ((hp = gethostbyname("142.232.48.31")) == NULL)
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