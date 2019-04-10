/*-------------------------------------------------------------------------------------
--	SOURCE FILE: request_handler.cpp - Contains request related functions
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					DWORD WINAPI SvrRequestReceiverThreadFunc(LPVOID lpParameter);
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
--	FUNCTION:	SvrRequestReceiverThreadFunc
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI SvrRequestReceiverThreadFunc(LPVOID lpParameter)
--									LPVOID lpParameter - request handler info struct
--
--	RETURNS:		DWORD
--
--	NOTES:
--	Request Receiver Thread function to wait for Accept Events and start receiving requests
--	from client
--------------------------------------------------------------------------------------*/
DWORD WINAPI SvrRequestReceiverThreadFunc(LPVOID lpParameter)
{
	LPTCP_SOCKET_INFO req_handler_info;
	WSAEVENT EventArray[1];
	DWORD Index;
	DWORD BytesSent = 0;

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
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				// An accept() call event is ready - break the wait loop
				break;
			}
		}

		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

		if (isAcceptingRequests)
		{
			update_server_msgs("Client connected");
			start_receiving_requests(req_handler_info->tcp_socket, req_handler_info->CompleteEvent, NULL);
		}
	}
	return TRUE;
}

DWORD WINAPI ClntReqReceiverThreadFunc(LPVOID lpParameter)
{
	LPTCP_SOCKET_INFO req_handler_info;
	WSAEVENT EventArray[1];
	DWORD Index;
	DWORD BytesSent = 0;

	req_handler_info = (LPTCP_SOCKET_INFO)lpParameter;

	// Save the accept event in the event array.
	EventArray[0] = req_handler_info->event;
	isAcceptingRequests = TRUE;

	start_receiving_requests(req_handler_info->tcp_socket, req_handler_info->CompleteEvent, req_handler_info->FtpCompleteEvent);

	while (isAcceptingRequests)
	{
		Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

		if (Index == WSA_WAIT_FAILED)
		{
			update_client_msgs("WSAWaitForMultipleEvents failed with error " + std::to_string(WSAGetLastError()));
			return FALSE;
		}
	}
	return TRUE;
}

void start_receiving_requests(SOCKET request_socket, WSAEVENT recvReqEvent, WSAEVENT ftpCompleteEvent)
{
	LPSOCKET_INFORMATION SocketInfo;
	DWORD Flags = 0;
	int retVal;
	DWORD RecvBytes;

	// Create a socket information structure to associate with the accepted socket.
	if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		update_server_msgs("GlobalAlloc() failed in Request Handler with error " + std::to_string(GetLastError()));
		terminate_connection();
		return;
	}

	// Fill in the details of our accepted socket.
	SocketInfo->Socket = request_socket;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	SocketInfo->BytesSEND = 0;
	SocketInfo->BytesRECV = 0;
	SocketInfo->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
	SocketInfo->DataBuf.buf = SocketInfo->Buffer;
	SocketInfo->CompletedEvent = recvReqEvent;
	SocketInfo->FtpCompletedEvent = ftpCompleteEvent;
	Flags = 0;

	retVal = WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), RequestReceiverRoutine);

	if (retVal == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			update_server_msgs("WSARecv failed in Request Handler with error " + std::to_string(WSAGetLastError()));
			terminate_connection();

			return;
		}
	}
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
		(Error == WSAECONNRESET) ? update_server_msgs("Client disconnected") : update_server_msgs("I/O operation failed in Request Handler with error");
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
		SI->BytesRECV = BytesTransferred;
		TriggerWSAEvent(SI->CompletedEvent);
	}
	else if (BytesTransferred > 0)
	{
		packet = request_buffer.peek();
		SI->BytesRECV += BytesTransferred;
		if (packet.length() < DEFAULT_REQUEST_PACKET_SIZE)
		{
			packet += SI->DataBuf.buf;
			request_buffer.update(packet);
			if (packet.length() == DEFAULT_REQUEST_PACKET_SIZE) 
			{
				//full packet  
				SI->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
				TriggerWSAEvent(SI->CompletedEvent);
			}
			else 
			{
				SI->DataBuf.len = (ULONG)(DEFAULT_REQUEST_PACKET_SIZE - packet.length());
			}
		}
	}

	/*if (SI->DataBuf.buf[0] == (FILE_LIST_TYPE + '0') && SI->BytesRECV == DEFAULT_REQUEST_PACKET_SIZE)
	{
		WaitForSingleObject(SI->FtpCompletedEvent, INFINITE);
	}*/

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
--					April 4, 2019 - JK - added additional request handling for client side
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
	EventArray[0] = (WSAEVENT)lpParameter;

	while (isAcceptingRequests)
	{
		while (isAcceptingRequests)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				update_server_msgs("WSAWaitForMultipleEvents failed in Request Handler with error");
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				break;
			}
		}
		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

		if (isAcceptingRequests)
		{
			if (!request_buffer.empty()) 
			{
				request = request_buffer.get();
				getPacketType(&parsedPacket, request);
				if (!request.empty())
				{
					switch (parsedPacket.type) 
					{
					case WAV_FILE_REQUEST_TYPE:
						parseRequest(&parsedPacket, request);
						update_server_msgs("Received file transfer request for " + parsedPacket.message);
						start_ftp(parsedPacket.message, parsedPacket.ip_addr);
						//start_ftp(parsedPacket.message, "192.168.0.12");
						break;
					case AUDIO_STREAM_REQUEST_TYPE:
						parseRequest(&parsedPacket, request);
						update_server_msgs("Received file stream request for " + parsedPacket.message);
						start_file_stream(parsedPacket.message, parsedPacket.port_num, parsedPacket.ip_addr);
						break;
					case VOIP_REQUEST_TYPE:
						// voip request
						parseRequest(&parsedPacket, request);
						start_voip(parsedPacket.port_num, parsedPacket.ip_addr);
						break;
					case STREAM_COMPLETE_TYPE:
						start_client_terminate_file_stream();
						break;
					case AUDIO_BUFFER_RDY_TYPE:
						resume_streaming();
						break;
					case FILE_LIST_TYPE:
						parseFileListRequest(&parsedPacket, request);
						setup_file_list_dropdown(parsedPacket.file_list);
						break;
					case FILE_LIST_REQUEST_TYPE:
						std::vector<std::string> list = get_file_list();
						std::string msg = generateReqPacketWithData(FILE_LIST_TYPE, list);
						send_request_to_clnt(msg);
						break;
					}
				}
			}
		}
	}

	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	getPacketType
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void getPacketType(LPREQUEST_PACKET parsedPacket, std::string packet)
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to get the type of the request
--------------------------------------------------------------------------------------*/
void getPacketType(LPREQUEST_PACKET parsedPacket, std::string packet)
{
	parsedPacket->type = packet.at(0) - '0';
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	parseRequest
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--					April 4, 2019 - removed packet type parsing to another function
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
	std::string temp_msg = packet.substr(1);
	size_t pos = 0;
	int i = 0;
	while ((pos = temp_msg.find(packetMsgDelimiterStr)) != std::string::npos) 
	{
		switch (i++) 
		{
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
--	FUNCTION:	parseFileListRequest
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void parseFileListRequest(LPREQUEST_PACKET parsedPacket, std::string packet)
--									LPREQUEST_PACKET parsedPacket - packet struct to be populated
--									std::string packet - the packet to parse
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to parse a request packet containing a list of files
--------------------------------------------------------------------------------------*/
void parseFileListRequest(LPREQUEST_PACKET parsedPacket, std::string packet)
{
	std::vector<std::string> list;
	std::string temp_msg = packet.substr(1);
	size_t pos = 0;
	int i = 0;
	while ((pos = temp_msg.find(packetMsgDelimiterStr)) != std::string::npos) 
	{
		list.push_back(temp_msg.substr(0, pos));
		temp_msg.erase(0, pos + packetMsgDelimiterStr.length());
	}
	parsedPacket->file_list = list;
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
	return std::to_string(type) + message + packetMsgDelimiterStr;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	generateReqPacketWithData
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		std::string generateReqPacketWithData(int type, std::vector<std::string> messages)
--									int type - the type of request
--									std::string message - request messages
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to generate a request packet with multiple messages
--------------------------------------------------------------------------------------*/
std::string generateReqPacketWithData(int type, std::vector<std::string> messages)
{
	std::string listStringified;
	listStringified += std::to_string(type);
	for (auto msg : messages)
	{
		listStringified += (msg + packetMsgDelimiterStr);
	}
	return listStringified;
}

void terminateRequestHandler()
{
	isAcceptingRequests = FALSE;
}