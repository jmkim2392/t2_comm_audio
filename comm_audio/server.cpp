/*-------------------------------------------------------------------------------------
--	SOURCE FILE: server.cpp - Contains server functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port);
--					void start_request_receiver();
--					void start_request_handler();
--					void start_broadcast();
--					DWORD WINAPI connection_monitor(LPVOID tcp_socket);
--					void start_ftp(std::string filename);
--					void start_file_stream(std::string filename, std::string client_port_num, std::string client_ip_addr);
--					void terminate_server();
--					void update_server_msgs(std::string message);
--					void setup_client_addr(SOCKADDR_IN* client_addr, std::string client_port, std::string client_ip_addr);
--					void resume_streaming();
--					void send_request_to_clnt(std::string msg);
--
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
#include "server.h"

namespace fs = std::filesystem;

SOCKET svr_tcp_accept_socket;
SOCKET udp_audio_socket;

SOCKET RequestSocket;
SOCKET multicast_Socket;

HANDLE AcceptThread;
HANDLE RequestReceiverThread;
HANDLE RequestHandlerThread;
HANDLE StreamingThread;
HANDLE BroadCastThread;

TCP_SOCKET_INFO tcp_socket_info;
BROADCAST_INFO bi;

SOCKADDR_IN InternetAddr;
SOCKADDR_IN client_addr_udp;
SOCKADDR_IN multicast_stLclAddr, multicast_stDstAddr;

struct ip_mreq stMreq;    /* Multicast interface structure */

WSADATA stWSAData;


BOOL isAcceptingConnections = FALSE;
BOOL isBroadcasting = FALSE;
WSAEVENT AcceptEvent;
WSAEVENT RequestReceivedEvent;
WSAEVENT StreamCompletedEvent;
HANDLE ResumeSendEvent;

HANDLE serverThreads[20];
int svr_threadCount = 0;
std::vector<HANDLE> svrThreads;

std::vector<std::string> server_msgs;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_server
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port)
--									LPCWSTR tcp_port - port number for tcp
--									LPCWSTR udp_port - port number for udp
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize the program as a server
--------------------------------------------------------------------------------------*/
void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port) 
{
	DWORD ThreadId;

	//open tcp socket 
	initialize_wsa(tcp_port, &InternetAddr);
	initialize_wsa_events(&AcceptEvent);
	initialize_wsa_events(&RequestReceivedEvent);
	initialize_wsa_events(&StreamCompletedEvent);
	initialize_events_gen(&ResumeSendEvent, L"ResumeSend");

	open_socket(&svr_tcp_accept_socket, SOCK_STREAM,IPPROTO_TCP);

	if (bind(svr_tcp_accept_socket, (PSOCKADDR)&InternetAddr,
		sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		printf("bind() failed with error %d\n", WSAGetLastError());
		update_server_msgs("Failed to bind tcp " + std::to_string(WSAGetLastError()));
		terminate_connection();
		return;
	}
	//open udp socket
	initialize_wsa(udp_port, &InternetAddr);
	open_socket(&udp_audio_socket, SOCK_DGRAM, IPPROTO_UDP);

	start_request_receiver();
	start_request_handler();
	start_broadcast();

	if ((AcceptThread = CreateThread(NULL, 0, connection_monitor, (LPVOID)&svr_tcp_accept_socket, 0, &ThreadId)) == NULL)
	{
		update_server_msgs("Failed to create AcceptThread " + std::to_string(GetLastError()));

		return;
	}


	add_new_thread_gen(svrThreads, AcceptThread);
	update_server_msgs("Server online..");
	update_status(connectedMsg);
}

void setup_client_addr(SOCKADDR_IN* client_addr, std::string client_port, std::string client_ip_addr)
{
	int port;
	char* port_num = (char *)malloc(MAX_INPUT_LENGTH);
	char* ip = (char *)malloc(MAX_INPUT_LENGTH);
	struct hostent	*hp;

	//wcstombs_s(&i, port_num, MAX_INPUT_LENGTH, client_port, MAX_INPUT_LENGTH);
	//wcstombs_s(&i, ip, MAX_INPUT_LENGTH, client_ip_addr, MAX_INPUT_LENGTH);
	port_num = (char*) client_port.c_str();
	ip = (char*) client_ip_addr.c_str();

	port = atoi(port_num);

	// Initialize and set up the address structure
	client_addr->sin_family = AF_INET;
	client_addr->sin_port = htons(port);

	if ((hp = gethostbyname(ip)) == NULL)
	{
		update_client_msgs("Server address unknown");
		update_status(disconnectedMsg);
		exit(1);
	}

	// Copy the server address
	memcpy((char *)&client_addr->sin_addr, hp->h_addr, hp->h_length);

}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_request_receiver
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_request_receiver() 
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start the request receiver which receives request packets from
--	clients
--------------------------------------------------------------------------------------*/
void start_request_receiver() 
{
	DWORD ThreadId;

	tcp_socket_info.event = AcceptEvent;
	tcp_socket_info.tcp_socket = RequestSocket;
	tcp_socket_info.CompleteEvent = RequestReceivedEvent;

	if ((RequestReceiverThread = CreateThread(NULL, 0, SvrRequestReceiverThreadFunc, (LPVOID)&tcp_socket_info, 0, &ThreadId)) == NULL)
	{
		update_server_msgs("Failed to create RequestReceiverThread " + std::to_string(GetLastError()));
		return;
	}
	add_new_thread_gen(svrThreads, RequestReceiverThread);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_request_handler
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_request_handler()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start the request handler thread which read and handle the packets
--	received from client
--------------------------------------------------------------------------------------*/
void start_request_handler()
{
	DWORD ThreadId;

	if ((RequestHandlerThread = CreateThread(NULL, 0, HandleRequest, (LPVOID)RequestReceivedEvent, 0, &ThreadId)) == NULL)
	{
		update_server_msgs("Failed to create RequestHandler Thread " + std::to_string(GetLastError()));
		return;
	}
	add_new_thread_gen(svrThreads, RequestHandlerThread);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_broadcast
--
--	DATE:			April 5, 2019
--
--	REVISIONS:		April 5, 2019
--
--	DESIGNER:		Phat Le
--
--	PROGRAMMER:		Phat Le
--
--	INTERFACE:		start_broadcast()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function start broadcast
--------------------------------------------------------------------------------------*/
void start_broadcast()
{

	DWORD ThreadId;

	char achMCAddr[MAXADDRSTR] = TIMECAST_ADDR;
	u_short nPort = TIMECAST_PORT;
	u_long  lTTL = TIMECAST_TTL;
	isBroadcasting = TRUE;

	if (!init_winsock(&stWSAData)) {
		isBroadcasting = FALSE;
		return;
	}

	if (!get_datagram_socket(&multicast_Socket)) {
		WSACleanup();
		isBroadcasting = FALSE;
		return;
	}

	if (!bind_socket(&multicast_stLclAddr, &multicast_Socket, 0)) {
		closesocket(multicast_Socket);
		WSACleanup();
		isBroadcasting = FALSE;
		return;
	}

	if (!join_multicast_group(&stMreq, &multicast_Socket, achMCAddr)) {
		closesocket(multicast_Socket);
		WSACleanup();
		isBroadcasting = FALSE;
		return;
	}

	if (!set_ip_ttl(&multicast_Socket, lTTL)) {
		closesocket(multicast_Socket);
		WSACleanup();
		isBroadcasting = FALSE;
		return;
	}

	if (!disable_loopback(&multicast_Socket)) {
		closesocket(multicast_Socket);
		WSACleanup();
		isBroadcasting = FALSE;
		return;
	}

	multicast_stDstAddr.sin_family = AF_INET;
	multicast_stDstAddr.sin_addr.s_addr = inet_addr(achMCAddr);
	multicast_stDstAddr.sin_port = htons(nPort);

	bi.hSocket = &multicast_Socket;
	bi.stDstAddr = &multicast_stDstAddr;


	if ((BroadCastThread = CreateThread(NULL, 0, broadcast_data, (LPVOID)&bi, 0, &ThreadId)) == NULL) {
		printf("CreateThread failed with error %d\n", GetLastError());
		closesocket(multicast_Socket);
		WSACleanup();
		isBroadcasting = FALSE;
		return;
	}
	add_new_thread_gen(svrThreads, BroadCastThread);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	connection_monitor
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI connection_monitor(LPVOID tcp_socket) 
--								LPVOID tcp_socket - the tcp_socket to open and listen
--	RETURNS:		DWORD
--
--	NOTES:
--	Thread function to listen for connection requests and trigger an Accept Event to
--	process connecting clients
--------------------------------------------------------------------------------------*/
DWORD WINAPI connection_monitor(LPVOID tcp_socket) {

	isAcceptingConnections = TRUE;

	SOCKET* socket = (SOCKET*)tcp_socket;

	if (listen(*socket, 5))
	{
		update_server_msgs("listen() failed with error " + std::to_string(WSAGetLastError()));
		return WSAGetLastError();
	}
	
	while (isAcceptingConnections)
	{
		tcp_socket_info.tcp_socket = accept(*socket, NULL, NULL);

		if (WSASetEvent(AcceptEvent) == FALSE)
		{
			update_server_msgs("WSASetEvent(AccentEvent) failed with error " + std::to_string(WSAGetLastError()));
			return WSAGetLastError();
		}
	}
	return 0;
}

///*-------------------------------------------------------------------------------------
//--	FUNCTION:	add_new_thread
//--
//--	DATE:			March 8, 2019
//--
//--	REVISIONS:		March 8, 2019
//--					March 14, 2019 - JK - Set for removal to use new function - SEE NOTES
//--
//--	DESIGNER:		Jason Kim
//--
//--	PROGRAMMER:		Jason Kim
//--
//--	INTERFACE:		void add_new_thread(DWORD threadId) 
//--								DWORD threadId - the threadId to add
//--
//--	RETURNS:		void
//--
//--	NOTES:
//--	DEPRECATED - USE function in general_functions
//--	Call this function to add a new thread to maintain the list of active threads
//--------------------------------------------------------------------------------------*/
//void add_new_thread(DWORD threadId) 
//{
//	serverThreads[svr_threadCount++] = threadId;
//}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_ftp
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_ftp(std::string filename)
--								std::string filename - name of the file to send to client
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to check if file requested by client exists and begin sending file
--	if doesn't exist, send file_not_found packet
--------------------------------------------------------------------------------------*/
void start_ftp(std::string filename) 
{
	update_server_msgs("Starting File Transfer");
	initialize_ftp(&tcp_socket_info.tcp_socket, NULL);
	(open_file(filename) == 0) ? start_sending_file() : send_file_not_found_packet();
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	start_file_stream
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void start_file_stream(std::string filename, std::string client_port_num, std::string client_ip_addr)
--										std::string filename - name of the file to send
--										std::string client_port_num - client's port number
--										std::string client_ip_addr - client's ip address
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start the file streaming process
--------------------------------------------------------------------------------------*/
void start_file_stream(std::string filename, std::string client_port_num, std::string client_ip_addr)
{
	DWORD ThreadId;
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	update_server_msgs("Starting File Stream");

	setup_client_addr(&client_addr_udp, client_port_num, client_ip_addr);

	// TODO: May or may not need to bind to socket to receive from client when VOIP connected, 
	//	 but when testing with localhost, binding to same address causes issues even with SO_REUSEADDR
	//	SO_REUSEADDR implementation may be flawed or can simply open a new port+socket to handle this in VOIP

	/*if (bind(udp_audio_socket, (struct sockaddr *)&client_addr_udp, sizeof(sockaddr)) == SOCKET_ERROR) {
		update_status(disconnectedMsg);
		update_server_msgs("Failed binding to udp socket" + std::to_string(WSAGetLastError()));
	}
	if (setsockopt(udp_audio_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_server_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}*/

	initialize_file_stream(&udp_audio_socket, &client_addr_udp, StreamCompletedEvent, ResumeSendEvent);

	if (open_file_to_stream(filename) == 0) {
		if ((StreamingThread = CreateThread(NULL, 0, SendStreamThreadFunc, (LPVOID)StreamCompletedEvent, 0, &ThreadId)) == NULL)
		{
			update_server_msgs("Failed to create Streaming Thread " + std::to_string(WSAGetLastError()));
			return;
		}
		add_new_thread_gen(svrThreads, StreamingThread);
	}
	else {
		update_server_msgs("Failed to open requested file " + std::to_string(GetLastError()));
		return;
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	update_server_msgs
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void update_server_msgs(std::string message)
--										std::string message - name of the file to send
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to update the server's messages on control panel
--------------------------------------------------------------------------------------*/
void update_server_msgs(std::string message)
{
	std::string cur_time = get_current_time();
	if (server_msgs.size() >= 9) {
		server_msgs.erase(server_msgs.begin());
	}
	server_msgs.push_back(cur_time + message);
	update_messages(server_msgs);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	resume_streaming
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void resume_streaming()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to trigger the SendFileStream Completion routine to resume
--------------------------------------------------------------------------------------*/
void resume_streaming() {
	TriggerEvent(ResumeSendEvent);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	send_request_to_clnt
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void send_request_to_clnt(std::string msg)
--									std::string msg - message to send to client
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to send a request packet to the connected client
--------------------------------------------------------------------------------------*/
void send_request_to_clnt(std::string msg)
{
	DWORD SendBytes;
	WSABUF DataBuf;
	OVERLAPPED Overlapped;

	DataBuf.buf = (char*)msg.c_str();
	DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;

	ZeroMemory(&Overlapped, sizeof(WSAOVERLAPPED));

	if (WSASend(tcp_socket_info.tcp_socket, &DataBuf, 1, &SendBytes, 0, &Overlapped, NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("Send Request failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	get_file_list
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void get_file_list()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to retrieve a list of wav files in the same directory of exe
--------------------------------------------------------------------------------------*/
std::vector<std::string> get_file_list()
{
	std::vector<std::string> file_list;
	std::string file;
	size_t pos = 0;

	for (const auto & entry : std::filesystem::directory_iterator(fs::current_path()))
	{
		file = entry.path().filename().string();
		if (file.find(wavExtension) != std::string::npos) {
			file_list.push_back(file);
		}
	}
	return file_list;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	terminate_server
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void terminate_server()
--
--	RETURNS:		void
--
--	NOTES:
--	TODO implement the rest of server cleanup functions to safely terminate program
--------------------------------------------------------------------------------------*/
void terminate_server()
{
	isAcceptingConnections = FALSE;

	// close all server's feature threads
	terminateAudioApi();
	terminateFtpHandler();
	terminateFileStream();
	terminateRequestHandler();

	// trigger all events to unblock threads
	TriggerWSAEvent(RequestReceivedEvent);

	// check if all client initialized threads have been terminated
	WaitForMultipleObjects((int)svrThreads.size(), svrThreads.data(), TRUE, INFINITE);

	// clear residual msgs
	server_msgs.clear();

	close_socket(&svr_tcp_accept_socket);
	close_socket(&udp_audio_socket);
	terminate_connection();
}