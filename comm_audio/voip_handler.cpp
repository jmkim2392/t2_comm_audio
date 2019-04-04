#include "voip_handler.h"


struct sockaddr_in receiving_client;
int receiving_client_len;


DWORD WINAPI ReceiverThreadFunc(LPVOID lpParameter)
{
	LPVOIP_INFO params = (LPVOIP_INFO)lpParameter;

	DWORD Index;
	WSAEVENT EventArray[1];
	BOOL isReceivingFileStream = TRUE;

	EventArray[0] = params->CompletedEvent;

	start_receiving_voip(params->Udp_Port);

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

void start_receiving_voip(LPCWSTR udp_port)
{
	DWORD RecvBytes;
	DWORD Flags = 0;
	SOCKET receiving_voip_socket;
	SOCKADDR_IN server_addr_udp;
	receiving_client_len = sizeof(sockaddr_in);

	BOOL isConnected = TRUE;
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	LPSOCKET_INFORMATION VoipSocketInfo;

	//open udp socket
	initialize_wsa(udp_port, &receiving_client);
	open_socket(&receiving_voip_socket, SOCK_DGRAM, IPPROTO_UDP);

	setup_svr_addr(&server_addr_udp, udp_port, L"192.168.1.73");

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
	VoipSocketInfo->Sock_addr = server_addr_udp;

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
	
	// not receiving anything anymore???
	// maybe put sending thread in completion routine??

	update_client_msgs("Received data");
	update_server_msgs("Received data");

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
	LPVOIP_INFO params = (LPVOIP_INFO)lpParameter;

	SOCKET sock;
	char buf[8192] = "hello";
	int data_size = 8192;
	struct sockaddr_in client;
	HOSTENT *hp;
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	// Create a datagram socket
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("Can't create a socket");
		exit(1);
	}

	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons((USHORT)params->Udp_Port);

	int client_len = sizeof(client);

	if ((hp = gethostbyname("192.168.1.73")) == NULL)
	{
		fprintf(stderr, "Can't get server's IP address\n");
		exit(1);
	}

	memcpy((char *)&client.sin_addr, hp->h_addr, hp->h_length);

	//set REUSEADDR for udp socket
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}

	while (TRUE)
	{
		// record audio?

		if (sendto(sock, buf, data_size, 0, (struct sockaddr *)&client, client_len) != data_size)
		{
			perror("sendto error");
			exit(1);
		}

		update_client_msgs("Sent data");
		update_server_msgs("Sent data");

		Sleep(1000);
	}

	closesocket(sock);
}