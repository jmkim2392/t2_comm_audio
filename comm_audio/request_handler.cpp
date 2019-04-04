/*-------------------------------------------------------------------------------------
--	SOURCE FILE: request_handler.cpp - Contains request related functions
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter);
--					void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred, 
--												LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					DWORD WINAPI HandleRequest(LPVOID lpParameter);
--					void parseRequest(LPREQUEST_PACKET parsedPacket, std::string packet);
--					void TriggerWSAEvent(WSAEVENT event);
--					std::string generateRequestPacket(int type, std::string message);
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
#include "request_handler.h"

bool isAcceptingRequests = FALSE;
circular_buffer<std::string> request_buffer(10);

/*-------------------------------------------------------------------------------------
--	FUNCTION:	RequestReceiverThreadFunc
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter)
--									LPVOID lpParameter - request handler info struct
--
--	RETURNS:		DWORD
--
--	NOTES:
--	Request Receiver Thread function to wait for Accept Events and start receiving requests
--	from client
--------------------------------------------------------------------------------------*/
DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter)
{
	LPSOCKET_INFORMATION SocketInfo;
	LPTCP_SOCKET_INFO req_handler_info;
	WSAEVENT EventArray[1];
	DWORD Index;
	DWORD RecvBytes;
	int retVal;
	DWORD BytesSent = 0;
	DWORD Flags = 0;

	req_handler_info = (LPTCP_SOCKET_INFO)lpParameter;

	// Save the accept event in the event array.
	EventArray[0] = req_handler_info->event;
	isAcceptingRequests = TRUE;

	while (isAcceptingRequests)
	{
		// Wait for accept() to signal an event and also process WorkerRoutine() returns.
		while (isAcceptingRequests)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				update_server_msgs("WSAWaitForMultipleEvents failed in Request Handler with error " + std::to_string(WSAGetLastError()));
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				// An accept() call event is ready - break the wait loop
				break;
			}
		}

		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

		update_server_msgs("Client connected");

		// Create a socket information structure to associate with the accepted socket.
		if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
			sizeof(SOCKET_INFORMATION))) == NULL)
		{
			update_server_msgs("GlobalAlloc() failed in Request Handler with error " + std::to_string(GetLastError()));
			terminate_connection();
			return FALSE;
		}

		// Fill in the details of our accepted socket.
		SocketInfo->Socket = req_handler_info->tcp_socket;
		ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
		SocketInfo->BytesSEND = 0;
		SocketInfo->BytesRECV = 0;
		SocketInfo->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
		SocketInfo->DataBuf.buf = SocketInfo->Buffer;
		SocketInfo->CompletedEvent = req_handler_info->CompleteEvent;

		Flags = 0;

		retVal = WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), RequestReceiverRoutine);

		if (retVal == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				update_server_msgs("WSARecv failed in Request Handler with error " + std::to_string(WSAGetLastError()));
				terminate_connection();

				return FALSE;
			}
		}
	}
	return TRUE;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	RequestReceiverRoutine
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred,
--												LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--										DWORD Error - WSA error
--										DWORD BytesTransferred - number of bytes transferred
--										LPWSAOVERLAPPED Overlapped - overlapped struct
--										DWORD InFlags - unused flags
--
--	RETURNS:		void
--
--	NOTES:
--	Completion Routine Callback function to receive request packets
--------------------------------------------------------------------------------------*/
void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;
	std::string packet;

	// Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0)
	{	
		(Error == WSAECONNRESET)? update_server_msgs("Client disconnected") : update_server_msgs("I/O operation failed in Request Handler with error");
	}

	if (BytesTransferred == 0)
	{
		update_server_msgs("Closing request socket " + std::to_string(SI->Socket));
	}

	if (Error != 0 || BytesTransferred == 0)
	{
		closesocket(SI->Socket);
		GlobalFree(SI);
		return;
	}

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	if (BytesTransferred == DEFAULT_REQUEST_PACKET_SIZE)
	{
		request_buffer.put(SI->DataBuf.buf);
 		SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
		TriggerWSAEvent(SI->CompletedEvent);
	}
	else if (BytesTransferred > 0) 
	{
		packet = request_buffer.peek();
		if (packet.length() < DEFAULT_REQUEST_PACKET_SIZE) 
		{
			packet += SI->DataBuf.buf;
			request_buffer.update(packet);
			if (packet.length() == DEFAULT_REQUEST_PACKET_SIZE) {
				//full packet  
				SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
				TriggerWSAEvent(SI->CompletedEvent);
			}
			else {
				SI->DataBuf.len = (ULONG) (DEFAULT_REQUEST_PACKET_SIZE - packet.length());
			}
		} 
	}

	SI->DataBuf.buf = SI->Buffer;
	
	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), RequestReceiverRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("WSARecv() failed in Request Handler with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	HandleRequest
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--					March 11, 2019 - JK - added packet parsing and request handling 
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI HandleRequest(LPVOID lpParameter)
--									LPVOID lpParameter - the Packet Received Event
--
--	RETURNS:		void
--
--	NOTES:
--	Thread function to read from circular buffer and handle requests as received
--------------------------------------------------------------------------------------*/
DWORD WINAPI HandleRequest(LPVOID lpParameter)
{
	std::string request;
	REQUEST_PACKET parsedPacket;
	DWORD Index;
	WSAEVENT EventArray[1];
	
	isAcceptingRequests = TRUE;
	EventArray[0] = (WSAEVENT) lpParameter;

	while (isAcceptingRequests)
	{
		while (isAcceptingRequests)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				update_server_msgs("WSAWaitForMultipleEvents failed in Request Handler with error");
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				// An accept() call event is ready - break the wait loop
				break;
			}
		}
		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

		if (!request_buffer.empty()) {
			request = request_buffer.get();
			if (!request.empty()) {
				parseRequest(&parsedPacket, request);

				switch (parsedPacket.type) {
				case WAV_FILE_REQUEST_TYPE:
					// audio file request
					// parsedPacket.message should contain the file name
					update_server_msgs("Received file transfer request for " + parsedPacket.message);
					start_ftp(parsedPacket.message);
					break;
				case AUDIO_STREAM_REQUEST_TYPE:
					// audio file stream request
					// parsedPacket.message should contain the file name
					update_server_msgs("Received file stream request for " + parsedPacket.message);

					// TODO: change hardcoded ip string with received client address
					start_file_stream(parsedPacket.message, parsedPacket.port_num, parsedPacket.ip_addr);
				
					break;
				case VOIP_REQUEST_TYPE:
					// voip request
					// parsedPacket.message should contain the client info
					start_voip();
					break;
				case AUDIO_BUFFER_FULL_TYPE:
					break;
				case AUDIO_BUFFER_RDY_TYPE:
					resume_streaming();
					break;
				}
			}
		}
	}
	
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	parseRequest
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void parseRequest(LPREQUEST_PACKET parsedPacket, std::string packet) 
--									LPREQUEST_PACKET parsedPacket - packet struct to be populated
--									std::string packet - the packet to parse
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to parse a string packet received into packet struct
--------------------------------------------------------------------------------------*/
void parseRequest(LPREQUEST_PACKET parsedPacket, std::string packet) 
{
	parsedPacket->type = packet.at(0) - '0';
	std::string temp_msg = packet.substr(1);
	size_t pos = 0;
	int i = 0;
	while ((pos = temp_msg.find(packetMsgDelimiterStr)) != std::string::npos) {
		switch (i++){
		case 0:
			parsedPacket->message = temp_msg.substr(0, pos);
			break;
		case 1:
			parsedPacket->ip_addr = temp_msg.substr(0, pos);
			break;
		case 2:
			parsedPacket->port_num = temp_msg.substr(0, pos);
			break;
		}
		temp_msg.erase(0, pos + packetMsgDelimiterStr.length());
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	generateRequestPacket
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		std::string generateRequestPacket(int type, std::string message)
--									int type - the type of request
--									std::string message - the request message
--
--	RETURNS:		std::string - request packet of type string
--
--	NOTES:
--	Call this function to generate a request packet of type string
--------------------------------------------------------------------------------------*/
std::string generateRequestPacket(int type, std::string message)
{
	return std::to_string(type) + message;
}