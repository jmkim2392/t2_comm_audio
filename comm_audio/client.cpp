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
SOCKET cl_udp_voip_receive_socket;
SOCKET cl_udp_voip_send_socket;
SOCKET cl_tcp_ftp_socket;
SOCKET ftp_receivingSock;

LPCWSTR tcp_port_num;
LPCWSTR udp_port_num;
LPCWSTR voip_send_udp_port_num;
LPCWSTR voip_receive_udp_port_num;
LPCWSTR tcp_ftp_port_num = L"4984";
std::wstring current_device_ip;

SOCKADDR_IN cl_addr;
SOCKADDR_IN server_addr_tcp;
SOCKADDR_IN server_addr_udp;
SOCKADDR_IN cl_udp_voip_sendto_addr;
SOCKADDR_IN cl_udp_voip_receive_addr;
SOCKADDR_IN voip_svr_addr_udp;
SOCKADDR_IN server_addr_tcp_ftp;
int server_len;

TCP_SOCKET_INFO clnt_tcp_socket_info;

HANDLE FileReceiverThread;
HANDLE FileStreamerThread;
HANDLE ClntRequestHandlerThread;
HANDLE ClntRequestReceiverThread;
HANDLE FtpAcceptThread;
FTP_INFO ftp_info;

std::vector<std::string> client_msgs;

std::vector<HANDLE> clntThreads;

BOOL isConnected = TRUE;
BOOL multicast_connected = FALSE;
BOOL isStreaming = FALSE;
BOOL isReceivingFile_Clnt = FALSE;
BOOL isFtpSocketReady = FALSE;

WSAEVENT FtpCompleted;
WSAEVENT FileStreamCompleted;
WSAEVENT ClntReqRecvEvent;
HANDLE DisconnectEvent;

BOOL bOptVal = FALSE;
int bOptLen = sizeof(BOOL);
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
	isConnected = TRUE;
	initialize_events_gen(&DisconnectEvent, L"Disconnect");
	initialize_wsa_events(&ClntReqRecvEvent);
	initialize_wsa_events(&FtpCompleted);
	
	current_device_ip = get_device_ip();
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// VOIP SETUP
	//open voip send socket
	//KTODO: remove port number hardcoding
	//voip_send_udp_port_num = L"4981";
	//initialize_wsa(voip_send_udp_port_num, &cl_udp_voip_sendto_addr);
	//open_socket(&cl_udp_voip_send_socket, SOCK_DGRAM, IPPROTO_UDP);
	//setup_svr_addr(&cl_udp_voip_sendto_addr, voip_send_udp_port_num, svr_ip_addr);

	//open voip receive socket
	voip_receive_udp_port_num = L"4982";
	initialize_wsa(voip_receive_udp_port_num, &cl_udp_voip_receive_addr);
	open_socket(&cl_udp_voip_receive_socket, SOCK_DGRAM, IPPROTO_UDP);
	setup_svr_addr(&cl_udp_voip_receive_addr, voip_receive_udp_port_num, current_device_ip.c_str());

	if (bind(cl_udp_voip_receive_socket, (struct sockaddr *)&cl_udp_voip_receive_addr, sizeof(sockaddr)) == SOCKET_ERROR) {
		update_client_msgs("Failed to bind voip udp socket " + std::to_string(WSAGetLastError()));
		update_status(disconnectedMsg);
	}
	//set REUSEADDR for udp socket
	if (setsockopt(cl_udp_voip_receive_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}
	if (setsockopt(cl_udp_voip_send_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// FTP SETUP
	// open tcp ftp socket 
	// tcp_ftp_port_num should be dynamically decremented from req port number; currently hardcoded
	initialize_wsa(tcp_ftp_port_num, &cl_addr);
	open_socket(&cl_tcp_ftp_socket, SOCK_STREAM, IPPROTO_TCP);
	setup_svr_addr(&server_addr_tcp_ftp, tcp_ftp_port_num, svr_ip_addr);
	// bind ftp socket
	if (bind(cl_tcp_ftp_socket, (PSOCKADDR)&cl_addr, sizeof(cl_addr)) == SOCKET_ERROR)
	{
		printf("bind() failed with error %d\n", WSAGetLastError());
		update_client_msgs("Failed to bind ftp tcp " + std::to_string(WSAGetLastError()));
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// FILE STREAM RECEIVER SETUP
	//save info to open udp socket later
	udp_port_num = udp_port;
	initialize_wsa(udp_port, &cl_addr);
	// TODO: make current dev IP consistent with hardcode localhost
	// This is for client receiver
	setup_svr_addr(&server_addr_udp, udp_port, current_device_ip.c_str());
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// REQUEST HANDLER SETUP
	// open tcp socket 
	tcp_port_num = tcp_port;
	initialize_wsa(tcp_port, &cl_addr);
	open_socket(&cl_tcp_req_socket, SOCK_STREAM, IPPROTO_TCP);
	setup_svr_addr(&server_addr_tcp, tcp_port, svr_ip_addr);
	// connect to tcp request socket
	if (connect(cl_tcp_req_socket, (struct sockaddr *)&server_addr_tcp, sizeof(sockaddr)) == -1)
	{
		update_client_msgs("Failed to connect to server " + std::to_string(WSAGetLastError()));
		isConnected = FALSE;
		update_status(disconnectedMsg);
	}

	start_client_request_receiver();
	start_client_request_handler();
	/////////////////////////////////////////////////////////////////////////////////////////////////////

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
		update_client_msgs("address unknown");
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

	update_client_msgs("Requesting file from server...");
	
	create_new_file("receivedWav.wav");

	update_client_msgs("Creating new file, receivedWav.wav");

	if ((FtpAcceptThread = CreateThread(NULL, 0, FtpThreadFunc, (LPVOID)&cl_tcp_ftp_socket, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed to create FtpThread " + std::to_string(GetLastError()));
		return;
	}

	std::wstring temp_msg = std::wstring(filename) + packetMsgDelimiter + current_device_ip + packetMsgDelimiter;
	LPCWSTR file_req_msg = temp_msg.c_str();

	send_request_to_svr(WAV_FILE_REQUEST_TYPE, file_req_msg);
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

	// setup Wave Out device for file stream
	WAVEFORMATEX wfx_fs_play;
	wfx_fs_play.nSamplesPerSec = 44100; /* sample rate */
	wfx_fs_play.wBitsPerSample = 16; /* sample size */
	wfx_fs_play.nChannels = 2; /* channels*/
	wfx_fs_play.cbSize = 0; /* size of _extra_ info */
	wfx_fs_play.wFormatTag = WAVE_FORMAT_PCM;
	wfx_fs_play.nBlockAlign = (wfx_fs_play.wBitsPerSample * wfx_fs_play.nChannels) >> 3;
	wfx_fs_play.nAvgBytesPerSec = wfx_fs_play.nBlockAlign * wfx_fs_play.nSamplesPerSec;
	initialize_waveout_device(wfx_fs_play, 0, AUDIO_BLOCK_SIZE);

	open_socket(&cl_udp_audio_socket, SOCK_DGRAM, IPPROTO_UDP);

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

	initialize_file_stream(&cl_udp_audio_socket, NULL, FileStreamCompleted, NULL);
	update_client_msgs("Requesting file stream from server...");

	if ((FileStreamerThread = CreateThread(NULL, 0, ReceiveStreamThreadFunc, (LPVOID)FileStreamCompleted, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed creating File Streamer Thread with error " + std::to_string(GetLastError()));
		return;
	}

	add_new_thread_gen(clntThreads, FileStreamerThread);

	std::wstring temp_msg = std::wstring(filename) + packetMsgDelimiter + current_device_ip + packetMsgDelimiter + udp_port_num + packetMsgDelimiter;
	LPCWSTR stream_req_msg = temp_msg.c_str();

	send_request_to_svr(AUDIO_STREAM_REQUEST_TYPE, stream_req_msg);
	isStreaming = TRUE;
}

void join_multicast_stream() {
	HANDLE multicast_receive_thread;
	DWORD ThreadId;
	multicast_connected = true;

	if ((multicast_receive_thread = CreateThread(NULL, 0, receive_data, &multicast_connected, 0, &ThreadId)) == NULL) {
		printf("CreateThread failed with error %d\n", GetLastError());
		WSACleanup();
		return;
	}
	add_new_thread_gen(clntThreads, multicast_receive_thread);
}

void disconnect_multicast() {
	multicast_connected = false;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	request_voip
--
--	DATE:			April 3, 2019
--
--	REVISIONS:		April 3, 2019
--
--	DESIGNER:		Dasha Strigoun
--
--	PROGRAMMER:		Dasha Strigoun
--
--	INTERFACE:		void request_voip()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to request and start the voip process
--------------------------------------------------------------------------------------*/
void request_voip(HWND voipHwndDlg)
{
	LPCWSTR receiving_port = L"4982";
	LPCWSTR sending_port = L"4981";
	LPCWSTR ip_addr = L"127.0.0.1";

	WSAEVENT VoipCompleted;
	initialize_wsa_events(&VoipCompleted);

	initialize_voip(&cl_udp_voip_receive_socket, &cl_udp_voip_send_socket, &cl_udp_voip_sendto_addr, VoipCompleted, NULL);

	// send request to voip
	update_client_msgs("Requesting VOIP from server...");
	// Make up a message to pass device ip address into request
	std::wstring temp_str = L"voip";
	std::wstring temp_msg = temp_str + packetMsgDelimiter + current_device_ip + packetMsgDelimiter + receiving_port + packetMsgDelimiter;
	LPCWSTR stream_req_msg = temp_msg.c_str();
	send_request_to_svr(VOIP_REQUEST_TYPE, stream_req_msg);

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
	add_new_thread_gen(clntThreads, ClntRequestReceiverThread);
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
	add_new_thread_gen(clntThreads, ClntRequestHandlerThread);
}

void reset_client_request_receiver() 
{
	TriggerWSAEvent(FtpCompleted);
	WSAResetEvent(FtpCompleted);
}

void start_client_terminate_file_stream() 
{
	if (isStreaming) {
		isStreaming = FALSE;
		terminateFileStream();
		close_socket(&cl_udp_audio_socket);
		close_popup();
	}
}

DWORD WINAPI FtpThreadFunc(LPVOID tcp_socket)
{
	DWORD ThreadId;
	
	SOCKET* socket = (SOCKET*)tcp_socket;

	isReceivingFile_Clnt = TRUE;

	if (!isFtpSocketReady)
	{
		if (listen(*socket, 5))
		{
			update_server_msgs("listen() failed with error " + std::to_string(WSAGetLastError()));
			return WSAGetLastError();
		}
		ftp_receivingSock = accept(*socket, NULL, NULL);
		isFtpSocketReady = TRUE;
	}

	initialize_ftp(&ftp_receivingSock, FtpCompleted);
	ftp_info.FtpCompleteEvent = FtpCompleted;

	if ((FileReceiverThread = CreateThread(NULL, 0, ReceiveFileThreadFunc, (LPVOID)&ftp_info, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed creating File Receiver Thread with error " + std::to_string(GetLastError()));
		return 0;
	}

	add_new_thread_gen(clntThreads, FileReceiverThread);
	return 0;
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
	// turn off all status flags to start terminating process
	isConnected = FALSE;
	isReceivingFile_Clnt = FALSE;
	isFtpSocketReady = FALSE;

	// close all client's feature threads
	terminateAudioApi();
	terminateFtpHandler();
	start_client_terminate_file_stream();
	terminateRequestHandler();

	// trigger all events to unblock threads
	TriggerWSAEvent(ClntReqRecvEvent);

	// check if all client initialized threads have been terminated
	WaitForMultipleObjects((int)clntThreads.size(), clntThreads.data(), TRUE, INFINITE);

	// clear residual msgs
	client_msgs.clear();

	// close client sockets 
	close_socket(&cl_tcp_req_socket);
	close_socket(&cl_tcp_ftp_socket);
	close_socket(&ftp_receivingSock);

	terminate_connection();
}