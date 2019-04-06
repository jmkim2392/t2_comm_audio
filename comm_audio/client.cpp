/*-------------------------------------------------------------------------------------
--	SOURCE FILE: client.cpp - Contains client functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_client(LPCWSTR tcp_port, LPCWSTR udp_port, LPCWSTR svr_ip_addr);
--					void setup_svr_addr(SOCKADDR_IN* svr_addr, LPCWSTR tcp_port, LPCWSTR svr_ip_addr);
--					void send_request_to_svr(int type, LPCWSTR request);
--					void terminate_client();
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--					March 24, 2019 - JK - Added FTP Feature
--					April 4, 2019 - JK - Added TCP request listener for client
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--
--------------------------------------------------------------------------------------*/
#include "client.h"

SOCKET cl_tcp_req_socket;
SOCKET cl_udp_audio_socket;

LPCWSTR tcp_port_num;
LPCWSTR udp_port_num;
std::wstring current_device_ip;

SOCKADDR_IN cl_addr;
SOCKADDR_IN server_addr_tcp;
SOCKADDR_IN server_addr_udp;
int server_len;

TCP_SOCKET_INFO clnt_tcp_socket_info;

HANDLE FileReceiverThread;
HANDLE FileStreamerThread;
HANDLE ClntRequestHandlerThread;
HANDLE ClntRequestReceiverThread;
FTP_INFO ftp_info;

std::vector<std::string> client_msgs;

DWORD clientThreads[20];
int cl_threadCount;

BOOL isConnected = TRUE;

WSAEVENT FtpCompleted;
WSAEVENT FileStreamCompleted;
WSAEVENT ClntReqRecvEvent;
HANDLE DisconnectEvent;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_client
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_client(LPCWSTR tcp_port, LPCWSTR udp_port, LPCWSTR svr_ip_addr)
--									LPCWSTR tcp_port - port number for tcp
--									LPCWSTR udp_port - port number for udp
--									LPCWSTR svr_ip_addr - hostname/ip of server
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize the program as a client
--------------------------------------------------------------------------------------*/
void initialize_client(LPCWSTR tcp_port, LPCWSTR udp_port, LPCWSTR svr_ip_addr)
{
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	initialize_events_gen(&DisconnectEvent, L"Disconnect");
	initialize_wsa_events(&ClntReqRecvEvent);
	initialize_wsa_events(&FtpCompleted);

	//open udp socket
	udp_port_num = udp_port;
	initialize_wsa(udp_port, &cl_addr);
	open_socket(&cl_udp_audio_socket, SOCK_DGRAM, IPPROTO_UDP);

	//open tcp socket 
	tcp_port_num = tcp_port;
	initialize_wsa(tcp_port, &cl_addr);
	open_socket(&cl_tcp_req_socket, SOCK_STREAM, IPPROTO_TCP);

	current_device_ip = get_device_ip();
	setup_svr_addr(&server_addr_tcp, tcp_port, svr_ip_addr);
	setup_svr_addr(&server_addr_udp, udp_port, current_device_ip.c_str());

	if (connect(cl_tcp_req_socket, (struct sockaddr *)&server_addr_tcp, sizeof(sockaddr)) == -1)
	{
		isConnected = FALSE;

		update_status(disconnectedMsg);
		update_client_msgs("Failed to connect to server " + std::to_string(WSAGetLastError()));
	}

	if (bind(cl_udp_audio_socket, (struct sockaddr *)&server_addr_udp, sizeof(sockaddr)) == SOCKET_ERROR) 
	{
		isConnected = FALSE;

		update_status(disconnectedMsg);
		update_client_msgs("Failed to bind udp socket " + std::to_string(WSAGetLastError()));
	}

	if (setsockopt(cl_udp_audio_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) 
	{
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}

	start_client_request_receiver();
	start_client_request_handler();

	initialize_audio_device();

	if (isConnected) 
	{
		update_client_msgs("Connected to server");
		update_status(connectedMsg);
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	setup_svr_addr
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void setup_svr_addr(SOCKADDR_IN* svr_addr , LPCWSTR tcp_port, LPCWSTR svr_ip_addr)
--									SOCKADDR_IN* svr_addr - socket struct to populate server info
--									LPCWSTR tcp_port - tcp port number
--									LPCWSTR svr_ip_addr - hostname/ip of server
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to setup the server socket information
--------------------------------------------------------------------------------------*/
void setup_svr_addr(SOCKADDR_IN* svr_addr, LPCWSTR svr_port, LPCWSTR svr_ip_addr)
{
	size_t i;
	int port;
	char* port_num = (char *)malloc(MAX_INPUT_LENGTH);
	char* ip = (char *)malloc(MAX_INPUT_LENGTH);
	struct hostent	*hp;

	wcstombs_s(&i, port_num, MAX_INPUT_LENGTH, svr_port, MAX_INPUT_LENGTH);
	wcstombs_s(&i, ip, MAX_INPUT_LENGTH, svr_ip_addr, MAX_INPUT_LENGTH);

	port = atoi(port_num);

	// Initialize and set up the address structure
	svr_addr->sin_family = AF_INET;
	svr_addr->sin_port = htons(port);

	if ((hp = gethostbyname(ip)) == NULL)
	{
		update_client_msgs("Server address unknown");
		update_status(disconnectedMsg);
	}

	// Copy the server address
	memcpy((char *)&svr_addr->sin_addr, hp->h_addr, hp->h_length);
	server_len = sizeof(&svr_addr);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	send_request_to_svr
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void send_request_to_svr(int type, LPCWSTR request)
--									int type - the type of request
--									LPCWSTR request - the request message
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to send a request to the server
--------------------------------------------------------------------------------------*/
void send_request_to_svr(int type, LPCWSTR request)
{
	size_t i;
	DWORD SendBytes;
	WSABUF DataBuf;
	OVERLAPPED Overlapped;
	char* req_msg = (char *)malloc(MAX_INPUT_LENGTH);
	std::string packet;

	wcstombs_s(&i, req_msg, MAX_INPUT_LENGTH, request, MAX_INPUT_LENGTH);

	packet = generateRequestPacket(type, req_msg);
	DataBuf.buf = (char*)packet.c_str();
	DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;

	ZeroMemory(&Overlapped, sizeof(WSAOVERLAPPED));

	if (WSASend(cl_tcp_req_socket, &DataBuf, 1, &SendBytes, 0,
		&Overlapped, NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("Send Request failed with error " + WSAGetLastError());
			update_status(disconnectedMsg);
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	request_wav_file
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void request_wav_file(LPCWSTR filename)
--									LPCWSTR filename - the name of file to request from server
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to request a wav file from server
--------------------------------------------------------------------------------------*/
void request_wav_file(LPCWSTR filename) 
{
	DWORD ThreadId;

	initialize_ftp(&cl_tcp_req_socket, FtpCompleted);
	update_client_msgs("Requesting file from server...");
	ftp_info.filename = filename;
	ftp_info.FtpCompleteEvent = FtpCompleted;

	create_new_file("receivedWav.wav");

	update_client_msgs("Creating new file, receivedWav.wav");

	if ((FileReceiverThread = CreateThread(NULL, 0, ReceiveFileThreadFunc, (LPVOID)&ftp_info, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed creating File Receiver Thread with error " + std::to_string(GetLastError()));
		return;
	}

	add_new_thread_gen(clientThreads, ThreadId, cl_threadCount++);

	send_request_to_svr(WAV_FILE_REQUEST_TYPE, filename);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	finalize_ftp
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void finalize_ftp(std::string msg)
--									std::string msg - msg to output
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to finalize ftp process
--------------------------------------------------------------------------------------*/
void finalize_ftp(std::string msg)
{
	enableButtons(TRUE);
	update_client_msgs(msg);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	request_file_stream
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void request_file_stream(LPCWSTR filename) 
--									LPCWSTR filename - name of the file
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to request a file stream and start the file stream process
--------------------------------------------------------------------------------------*/
void request_file_stream(LPCWSTR filename) 
{
	DWORD ThreadId;
	initialize_wsa_events(&FileStreamCompleted);

	initialize_file_stream(&cl_udp_audio_socket, NULL, FileStreamCompleted, NULL);
	update_client_msgs("Requesting file stream from server...");

	if ((FileStreamerThread = CreateThread(NULL, 0, ReceiveStreamThreadFunc, (LPVOID)FileStreamCompleted, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed creating File Streamer Thread with error " + std::to_string(GetLastError()));
		return;
	}

	add_new_thread_gen(clientThreads, ThreadId, cl_threadCount++);

	std::wstring temp_msg = std::wstring(filename) + packetMsgDelimiter + current_device_ip + packetMsgDelimiter + udp_port_num + packetMsgDelimiter;
	LPCWSTR stream_req_msg = temp_msg.c_str();

	send_request_to_svr(AUDIO_STREAM_REQUEST_TYPE, stream_req_msg);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	update_client_msgs
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void update_client_msgs(std::string message)
--									std::string message - message to output
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to output new messages to the Client's Control Panel
--------------------------------------------------------------------------------------*/
void update_client_msgs(std::string message)
{
	std::string cur_time = get_current_time();
	if (client_msgs.size() >= 6) 
	{
		client_msgs.erase(client_msgs.begin());
	}
	client_msgs.push_back(cur_time + message);
	update_messages(client_msgs);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_client_request_receiver
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_client_request_receiver()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start listening for requests from server
--------------------------------------------------------------------------------------*/
void start_client_request_receiver()
{
	DWORD ThreadId;

	clnt_tcp_socket_info.tcp_socket = cl_tcp_req_socket;
	clnt_tcp_socket_info.CompleteEvent = ClntReqRecvEvent;
	clnt_tcp_socket_info.event = DisconnectEvent;
	clnt_tcp_socket_info.FtpCompleteEvent = FtpCompleted;

	if ((ClntRequestReceiverThread = CreateThread(NULL, 0, ClntReqReceiverThreadFunc, (LPVOID)&clnt_tcp_socket_info, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed to create RequestReceiverThread " + std::to_string(GetLastError()));
		return;
	}
	add_new_thread(ThreadId);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_client_request_handler
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_client_request_handler()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start the request handler thread for client
--------------------------------------------------------------------------------------*/
void start_client_request_handler()
{
	DWORD ThreadId;

	if ((ClntRequestHandlerThread = CreateThread(NULL, 0, HandleRequest, (LPVOID)ClntReqRecvEvent, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed to create RequestHandler Thread " + std::to_string(GetLastError()));
		return;
	}
	add_new_thread(ThreadId);
}

void reset_client_request_receiver() 
{
	TriggerWSAEvent(FtpCompleted);
	WSAResetEvent(FtpCompleted);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	terminate_client
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void terminate_client()
--
--	RETURNS:		void
--
--	NOTES:
--	TODO implement the rest of client cleanup functions to safely terminate program
--------------------------------------------------------------------------------------*/
void terminate_client()
{
	// free any allocated resources
	// close all client threads
	isConnected = FALSE;
	// close client sockets 
}