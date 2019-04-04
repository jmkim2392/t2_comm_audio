/*-------------------------------------------------------------------------------------
--	SOURCE FILE: voip_handler.cpp - Contains voip functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					DWORD WINAPI ReceiverThreadFunc(LPVOID lpParameter)
--					void start_receiving_voip(LPCWSTR udp_port)
--					void CALLBACK Voip_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--					DWORD WINAPI SenderThreadFunc(LPVOID lpParameter)
--					
--					
--
--	DATE:			April 3, 2019
--
--	REVISIONS:		April 3, 2019
--
--	DESIGNER:		Dasha Strigoun
--
--	PROGRAMMER:		Dasha Strigoun
--
--
--------------------------------------------------------------------------------------*/
#include "voip_handler.h"


struct sockaddr_in receiving_client;
int receiving_client_len;

// NOTES:
// - commit 6202f72363b3804812cc719bd9d4b1a75f8b6977 has server receiving and client sending successfully
// - this version has sending, but receiving doesn't hit completion routine for some reason

// - ^ reason was the port; hardcoded port works fine
// - tried tracking the port on the client & server side.. looks fine to me....
//		-> try tracking in server, client, ReceiverThread, SenderThread, start_voip, & completion routine

/*-------------------------------------------------------------------------------------
--	FUNCTION:	ReceiverThreadFunc
--
--	DATE:			April 3, 2019
--
--	REVISIONS:		April 3, 2019
--
--	DESIGNER:		Dasha Strigoun
--
--	PROGRAMMER:		Dasha Strigoun
--
--	INTERFACE:		DWORD WINAPI SendStreamThreadFunc(LPVOID lpParameter)
--										LPVOID lpParameter - a LPVOIP_INFO struct that contains the event that
--										signifies completion and the UDP port number
--
--	RETURNS:		DWORD - 0 Thread terminated
--
--	NOTES:
--	Thread function for receiving VOIP packets
--------------------------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_receiving_voip
--
--	DATE:			April 3, 2019
--
--	REVISIONS:		April 3, 2019
--
--	DESIGNER:		Dasha Strigoun
--
--	PROGRAMMER:		Dasha Strigoun
--
--	INTERFACE:		void start_receiving_voip(LPCWSTR udp_port)
--									LPCWSTR udp_port - UDP socket port number
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to receive voip packets
--------------------------------------------------------------------------------------*/
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

	//LPCWSTR temp_udp_port = L"4981";

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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	Voip_ReceiveRoutine
--
--	DATE:			April 3, 2019
--
--	REVISIONS:		April 3, 2019
--
--	DESIGNER:		Dasha Strigoun
--
--	PROGRAMMER:		Dasha Strigoun
--
--	INTERFACE:		void CALLBACK FileStream_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--
--	RETURNS:		void
--
--	NOTES:
--	Completion routine for receiving voip packets
--------------------------------------------------------------------------------------*/
void CALLBACK Voip_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
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

	//USHORT udp_port = 4981;
	USHORT udp_port = (USHORT)params->Udp_Port;

	// Create a datagram socket
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("Can't create a socket");
		exit(1);
	}

	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(udp_port);


	int client_len = sizeof(client);

	if ((hp = gethostbyname("142.232.48.31")) == NULL)
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

		//Sleep() is for slowing it down a little; normally filling up the buffer will slow it down
		Sleep(1000);
	}

	closesocket(sock);
}