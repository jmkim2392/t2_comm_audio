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


struct sockaddr_in sender_addr;
int sender_addr_len;

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

	start_receiving_voip(params->Ip_addr, params->Udp_Port);

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
void start_receiving_voip(LPCWSTR ip_addr, LPCWSTR udp_port)
{
	DWORD RecvBytes;
	DWORD Flags = 0;
	SOCKET receiving_voip_socket;
	SOCKADDR_IN curr_addr;
	sender_addr_len = sizeof(sockaddr_in);

	BOOL isConnected = TRUE;
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	LPSOCKET_INFORMATION VoipSocketInfo;

	//open udp socket
	initialize_wsa(udp_port, &sender_addr);
	open_socket(&receiving_voip_socket, SOCK_DGRAM, IPPROTO_UDP);

	setup_svr_addr(&curr_addr, udp_port, ip_addr);

	//bind to server address
	if (bind(receiving_voip_socket, (struct sockaddr *)&curr_addr, sizeof(sockaddr)) == SOCKET_ERROR) {
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
	VoipSocketInfo->Sock_addr = curr_addr;

	if (WSARecvFrom(VoipSocketInfo->Socket, &(VoipSocketInfo->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& sender_addr, &sender_addr_len, &(VoipSocketInfo->Overlapped), Voip_ReceiveRoutine) == SOCKET_ERROR)
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

	OutputDebugStringA("Recv CRoutine\n");
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

	memset(SI->DataBuf.buf, 0, SI->DataBuf.len);

	SI->DataBuf.buf = SI->AUDIO_BUFFER;
	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& sender_addr, &sender_addr_len, &(SI->Overlapped), Voip_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecv() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	SenderThreadFunc
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
--	Thread function for sending VOIP packets
--------------------------------------------------------------------------------------*/
DWORD WINAPI SenderThreadFunc(LPVOID lpParameter)
{
	LPVOIP_INFO params = (LPVOIP_INFO)lpParameter;

	SOCKET sending_voip_socket;
	struct sockaddr_in connect_addr;
	int connect_addr_len;
	HOSTENT *hp;
	char ip_addr[MAX_PATH];

	char buf[8192] = "hello";
	int data_size = 8192;
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	// Convert udp_port from LPCWSTR to USHORT
	LPCWSTR str_udp_port = (LPCWSTR)params->Udp_Port;
	int int_udp_port = _wtoi(str_udp_port);
	USHORT udp_port = (USHORT)int_udp_port;

	// Create a datagram socket
	if ((sending_voip_socket = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("Can't create a socket");
		exit(1);
	}

	// Set up address of device to send to
	memset((char *)&connect_addr, 0, sizeof(connect_addr));
	connect_addr.sin_family = AF_INET;
	connect_addr.sin_port = htons(udp_port);
	connect_addr_len = sizeof(connect_addr);

	// Convert ip address from LPCWSTR to const char*
	//WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, params->Ip_addr, -1, ip_addr, sizeof(ip_addr), NULL, NULL);

	// Get host name of ip address
	//if ((hp = gethostbyname(ip_addr)) == NULL)
	if ((hp = gethostbyname("localhost")) == NULL)
	{
		fprintf(stderr, "Can't get server's IP address\n");
		exit(1);
	}
	memcpy((char *)&connect_addr.sin_addr, hp->h_addr, hp->h_length);

	//set REUSEADDR for udp socket
	if (setsockopt(sending_voip_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}

	// start audio recording thread
	HANDLE ReadyToSendEvent;
	initialize_events_gen(&ReadyToSendEvent, L"AudioSendReady");
	//startRecording(ReadyToSendEvent);

	while (TRUE)
	{
		// wait for block to fill up
		//WaitForSingleObject(ReadyToSendEvent, INFINITE);
		//ResetEvent(ReadyToSendEvent);

		OutputDebugStringA("hello2");
		//getRecordedAudioBuffer();

		if (sendto(sending_voip_socket, buf, data_size, 0, (struct sockaddr *)&connect_addr, connect_addr_len) != data_size)
		{
			perror("sendto error");
			exit(1);
		}

		update_client_msgs("Sent data");
		update_server_msgs("Sent data");

		//Sleep() is for slowing it down a little; normally filling up the buffer will slow it down
		Sleep(1000);
	}

	closesocket(sending_voip_socket);
}