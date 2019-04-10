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
int voipPacketNumRecv = 0;

char *voip_send_buf;
LPSOCKET_INFORMATION VoipSendSocketInfo;
LPSOCKET_INFORMATION VoipReceiveSocketInfo;


void initialize_voip(SOCKET* receive_socket, SOCKET* send_socket, SOCKADDR_IN* addr, WSAEVENT voipSendCompletedEvent, HANDLE eventTrigger)
{
	initialize_voip_send(send_socket, addr, voipSendCompletedEvent, eventTrigger);
	initialize_voip_receive(receive_socket, addr, NULL, NULL);
}

void initialize_voip_receive(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT voipReceiveCompletedEvent, HANDLE eventTrigger)
{
	// Create a socket information structure
	if ((VoipReceiveSocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		//terminate_connection();
		return;
	}

	// Fill in the details of the server socket to receive file from
	VoipReceiveSocketInfo->Socket = *socket;
	ZeroMemory(&(VoipReceiveSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	VoipReceiveSocketInfo->BytesSEND = 0;
	VoipReceiveSocketInfo->BytesRECV = 0;
	if (addr != NULL)
	{
		VoipReceiveSocketInfo->Sock_addr = *addr;
	}

	// KTODO: remove buf.len hardcode. this same as audio send buffer block size;
	VoipReceiveSocketInfo->DataBuf.len = VOIP_BLOCK_SIZE;
	VoipReceiveSocketInfo->DataBuf.buf = VoipReceiveSocketInfo->AUDIO_BUFFER;
	if (voipReceiveCompletedEvent != NULL)
	{
		VoipReceiveSocketInfo->CompletedEvent = voipReceiveCompletedEvent;
	}
	if (eventTrigger != NULL)
	{
		VoipReceiveSocketInfo->EventTrigger = eventTrigger;
	}
}

void initialize_voip_send(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT voipSendCompletedEvent, HANDLE eventTrigger)
{
	//file_stream_buf = (char*)malloc(AUDIO_PACKET_SIZE);

	// Create a socket information structure
	if ((VoipSendSocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		//terminate_connection();
		return;
	}

	// Fill in the details of the server socket to receive file from
	VoipSendSocketInfo->Socket = *socket;
	ZeroMemory(&(VoipSendSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	VoipSendSocketInfo->BytesSEND = 0;
	VoipSendSocketInfo->BytesRECV = 0;
	if (addr != NULL) 
	{
		VoipSendSocketInfo->Sock_addr = *addr;
	}

	// KTODO: remove buf.len hardcode. this same as audio send buffer block size;
	VoipSendSocketInfo->DataBuf.len = VOIP_BLOCK_SIZE;
	VoipSendSocketInfo->DataBuf.buf = VoipSendSocketInfo->AUDIO_BUFFER;
	if (voipSendCompletedEvent != NULL)
	{
		VoipSendSocketInfo->CompletedEvent = voipSendCompletedEvent;
	}
	if (eventTrigger != NULL)
	{
		VoipSendSocketInfo->EventTrigger = eventTrigger;
	}
}





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

	start_receiving_voip();

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
void start_receiving_voip()
{
	DWORD RecvBytes;
	DWORD Flags = 0;
	sender_addr_len = sizeof(sockaddr_in);

	OutputDebugStringA("wait for data\n");
	if (WSARecvFrom(VoipReceiveSocketInfo->Socket, &(VoipReceiveSocketInfo->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& sender_addr, &sender_addr_len, &(VoipReceiveSocketInfo->Overlapped), Voip_ReceiveRoutine) == SOCKET_ERROR)
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
	// Need to have almost same contents as FileStream_ReceiveRoutine
	DWORD RecvBytes;
	DWORD Flags;

	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;
	if (Error != 0)
	{
		update_client_msgs("I/O operation failed with error " + std::to_string(Error));
	}

	if (BytesTransferred != VOIP_BLOCK_SIZE) {
		return;
	}

	voipPacketNumRecv++;
	update_client_msgs("PacketRecv: " + std::to_string(voipPacketNumRecv));
	update_server_msgs("PacketRecv: " + std::to_string(voipPacketNumRecv));

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	writeToAudioBuffer(SI->DataBuf.buf, VOIP_BLOCK_SIZE);

	SI->DataBuf.buf = SI->VOIP_RECV_BUFFER;
	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& sender_addr, &sender_addr_len, &(SI->Overlapped), Voip_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecv() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

void send_audio_block(PWAVEHDR pwhdr)
{
	//KTODO: Remove hardcode byte for data_size, now this hasn't been used. may be used for error check
	int data_size = 44100;

	size_t n;

	n = sendto(VoipSendSocketInfo->Socket, pwhdr->lpData, pwhdr->dwBytesRecorded, 0, (SOCKADDR *)&(VoipSendSocketInfo->Sock_addr), sizeof(VoipSendSocketInfo->Sock_addr));
	char sbuf[512];
	sprintf_s(sbuf, "Sent: %d bytes\n", n);
	update_client_msgs(sbuf);

	wave_in_add_buffer(pwhdr, sizeof(WAVEHDR));
}
