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
--					void start_voip(std::string client_port_num, std::string client_ip_addr);
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
SOCKET udp_voip_receive_socket;
SOCKET udp_voip_send_socket;
SOCKET svr_tcp_ftp_socket;

SOCKET RequestSocket;
SOCKET multicast_Socket;

HANDLE AcceptThread;
HANDLE RequestReceiverThread;
HANDLE RequestHandlerThread;
HANDLE StreamingThread;
HANDLE BroadCastThread;

LPCWSTR svr_tcp_ftp_port = L"4984";
std::string svr_tcp_ftp_port_str = "4984";

TCP_SOCKET_INFO tcp_socket_info;
BROADCAST_INFO bi;

SOCKADDR_IN svr_req_handler_addr;
SOCKADDR_IN svr_file_stream_addr;
SOCKADDR_IN svr_udp_voip_receive_addr;
SOCKADDR_IN svr_udp_voip_sendto_addr;
SOCKADDR_IN client_addr_tcp_ftp;

LPCWSTR voip_send_port_num;
LPCWSTR voip_receive_port_num;

SOCKADDR_IN multicast_stLclAddr, multicast_stDstAddr;

struct ip_mreq stMreq;    /* Multicast interface structure */

WSADATA stWSAData;

BOOL isAcceptingConnections = FALSE;
BOOL isBroadcasting = FALSE;
BOOL ftpSocketReady = FALSE;
WSAEVENT AcceptEvent;
WSAEVENT RequestReceivedEvent;
WSAEVENT StreamCompletedEvent;
WSAEVENT VoipCompleted;

HANDLE ResumeSendEvent;
HANDLE SvrMulticastResumeSendEvent;

HANDLE serverThreads[20];
int svr_threadCount = 0;
std::vector<HANDLE> svrThreads;

std::vector<std::string> server_msgs;

std::wstring svr_device_ip;
std::string svr_device_ip_str;

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
	BOOL bOptVal = FALSE;
	int bOptLen = sizeof(BOOL);

	initialize_wsa_events(&AcceptEvent);
	initialize_wsa_events(&RequestReceivedEvent);
	initialize_wsa_events(&StreamCompletedEvent);
	initialize_wsa_events(&VoipCompleted);
	initialize_events_gen(&ResumeSendEvent, L"ResumeSend");

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// REQUEST HANDLER SETUP
	//open tcp socket 
	initialize_wsa(tcp_port, &svr_req_handler_addr);
	open_socket(&svr_tcp_accept_socket, SOCK_STREAM, IPPROTO_TCP);

	if (bind(svr_tcp_accept_socket, (PSOCKADDR)&svr_req_handler_addr,
		sizeof(svr_req_handler_addr)) == SOCKET_ERROR)
	{
		update_server_msgs("Failed to bind tcp " + std::to_string(WSAGetLastError()));
		terminate_connection();
		return;
	}

	start_request_receiver();
	start_request_handler();
	if ((AcceptThread = CreateThread(NULL, 0, connection_monitor, (LPVOID)&svr_tcp_accept_socket, 0, &ThreadId)) == NULL)
	{
		update_server_msgs("Failed to create AcceptThread " + std::to_string(GetLastError()));

		return;
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	svr_device_ip = get_device_ip();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &svr_device_ip[0], (int)svr_device_ip.size(), NULL, 0, NULL, NULL);
	std::string temp_ip_str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &svr_device_ip[0], (int)svr_device_ip.size(), &temp_ip_str[0], size_needed, NULL, NULL);
	svr_device_ip_str = temp_ip_str;
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// VOIP SETUP
	//open udp send socket
	voip_send_port_num = L"4982";
	initialize_wsa(voip_send_port_num, &svr_udp_voip_sendto_addr);
	open_socket(&udp_voip_send_socket, SOCK_DGRAM, IPPROTO_UDP);
	// setup_client_addr happens when start_voip is called after voip request is received by server

	////open udp receive socket
	voip_receive_port_num = L"4981";
	initialize_wsa(voip_receive_port_num, &svr_udp_voip_receive_addr);
	open_socket(&udp_voip_receive_socket, SOCK_DGRAM, IPPROTO_UDP);
	setup_client_addr(&svr_udp_voip_receive_addr, "4981", svr_device_ip_str);

	if (bind(udp_voip_receive_socket, (struct sockaddr *)&svr_udp_voip_receive_addr, sizeof(sockaddr)) == SOCKET_ERROR) {
		update_server_msgs("Failed to bind voip udp socket " + std::to_string(WSAGetLastError()));
		update_status(disconnectedMsg);
	}
	//set REUSEADDR for udp socket
	if (setsockopt(udp_voip_receive_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_server_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}
	if (setsockopt(udp_voip_send_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// FTP SETUP
	//open tcp ftp socket 
	// tcp_ftp_port_num should be dynamically decremented from req port number; currently hardcoded
	initialize_wsa(svr_tcp_ftp_port, &client_addr_tcp_ftp);
	open_socket(&svr_tcp_ftp_socket, SOCK_STREAM, IPPROTO_TCP);
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// FILE STREAM SETUP
	//open udp socket
	initialize_wsa(udp_port, &svr_file_stream_addr);
	open_socket(&udp_audio_socket, SOCK_DGRAM, IPPROTO_UDP);
	
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	initialize_events_gen(&SvrMulticastResumeSendEvent, L"SvrMulticastEvent");
	start_broadcast();

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
	port_num = (char*)client_port.c_str();
	ip = (char*)client_ip_addr.c_str();

	port = atoi(port_num);

	// Initialize and set up the address structure
	client_addr->sin_family = AF_INET;
	client_addr->sin_port = htons(port);

	if ((hp = gethostbyname(ip)) == NULL)
	{
		update_server_msgs("Server address unknown");
		update_status(disconnectedMsg);
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
	setup_svr_multicast(SvrMulticastResumeSendEvent);

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
	bi.SendNextEvent = SvrMulticastResumeSendEvent;

	// setup wave out device for MULTICAST SERVER
	WAVEFORMATEX svr_wfx_multicast_play;
	svr_wfx_multicast_play.nSamplesPerSec = 11025; /* sample rate */
	svr_wfx_multicast_play.wBitsPerSample = 8; /* sample size */
	svr_wfx_multicast_play.nChannels = 2; /* channels*/
	svr_wfx_multicast_play.cbSize = 0; /* size of _extra_ info */
	svr_wfx_multicast_play.wFormatTag = WAVE_FORMAT_PCM;
	svr_wfx_multicast_play.nBlockAlign = (svr_wfx_multicast_play.wBitsPerSample * svr_wfx_multicast_play.nChannels) >> 3;
	svr_wfx_multicast_play.nAvgBytesPerSec = svr_wfx_multicast_play.nBlockAlign * svr_wfx_multicast_play.nSamplesPerSec;

	initialize_waveout_device(svr_wfx_multicast_play, 1, AUDIO_BLOCK_SIZE);
	change_device_volume((DWORD)MAKELONG(0x0000, 0x0000));
	
	

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
void start_ftp(std::string filename, std::string ip_addr)
{
	LPWSTR ip = new WCHAR[ip_addr.length() + 1];

	::MultiByteToWideChar(CP_ACP, 0, ip_addr.c_str(), (int)ip_addr.size(), ip, (int)ip_addr.length());

	ip[ip_addr.length()] = 0;

	//setup_svr_addr(&client_addr_tcp_ftp, svr_tcp_ftp_port, ip);
	setup_client_addr(&client_addr_tcp_ftp, svr_tcp_ftp_port_str, ip_addr);

	if (!ftpSocketReady)
	{
		// connect to tcp request socket
		if (connect(svr_tcp_ftp_socket, (struct sockaddr *)&client_addr_tcp_ftp, sizeof(sockaddr)) == -1)
		{
			update_status(disconnectedMsg);
			update_server_msgs("Failed to connect to client for ftp " + std::to_string(WSAGetLastError()));
		}
		ftpSocketReady = TRUE;
	}

	update_server_msgs("Starting File Transfer");
	initialize_ftp(&svr_tcp_ftp_socket, NULL);
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

	setup_client_addr(&svr_file_stream_addr, client_port_num, client_ip_addr);

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

	initialize_file_stream(&udp_audio_socket, &svr_file_stream_addr, StreamCompletedEvent, ResumeSendEvent);

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
--	FUNCTION:	start_voip
--
--	DATE:			April 3, 2019
--
--	REVISIONS:		April 3, 2019
--
--	DESIGNER:		Dasha Strigoun
--
--	PROGRAMMER:		Dasha Strigoun
--
--	INTERFACE:		void start_voip()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to start the voip process
--------------------------------------------------------------------------------------*/
void start_voip(std::string client_port_num, std::string client_ip_addr)
{
	//start_Server_Stream();

	LPCWSTR receiving_port = L"4981";
	LPCWSTR sending_port = L"4982";
	LPCWSTR ip_addr = L"127.0.0.1";

	setup_client_addr(&svr_udp_voip_sendto_addr, client_port_num, client_ip_addr);
	initialize_voip(&udp_voip_receive_socket, &udp_voip_send_socket, &svr_udp_voip_sendto_addr, VoipCompleted, NULL);

	// setup wave out device for VOIP
	WAVEFORMATEX wfx_voip_play;
	wfx_voip_play.nSamplesPerSec = 11025; /* sample rate */
	wfx_voip_play.wBitsPerSample = 8; /* sample size */
	wfx_voip_play.nChannels = 2; /* channels*/
	wfx_voip_play.cbSize = 0; /* size of _extra_ info */
	wfx_voip_play.wFormatTag = WAVE_FORMAT_PCM;
	wfx_voip_play.nBlockAlign = (wfx_voip_play.wBitsPerSample * wfx_voip_play.nChannels) >> 3;
	wfx_voip_play.nAvgBytesPerSec = wfx_voip_play.nBlockAlign * wfx_voip_play.nSamplesPerSec;

	initialize_waveout_device(wfx_voip_play, 0, VOIP_BLOCK_SIZE);


	// struct with VoipCompleted event and port
	LPVOIP_INFO receiving_thread_params;
	if ((receiving_thread_params = (LPVOIP_INFO)GlobalAlloc(GPTR,
		sizeof(VOIP_INFO))) == NULL)
	{
		//terminate_connection();
		return;
	}
	receiving_thread_params->CompletedEvent = VoipCompleted;
	receiving_thread_params->Ip_addr = ip_addr;
	receiving_thread_params->Udp_Port = receiving_port;

	HANDLE ReceiverThread;
	DWORD ReceiverThreadId;
	if ((ReceiverThread = CreateThread(NULL, 0, ReceiverThreadFunc, (LPVOID)receiving_thread_params, 0, &ReceiverThreadId)) == NULL)
	{
		update_client_msgs("Failed creating Voip Receiving Thread with error " + std::to_string(GetLastError()));
		return;
	}

	startRecording();
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
	ftpSocketReady = FALSE;

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
	close_socket(&svr_tcp_ftp_socket);
	terminate_connection();
}