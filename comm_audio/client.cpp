/*-------------------------------------------------------------------------------------
--	SOURCE FILE: client.cpp - Contains client functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_client(LPCWSTR tcp_port, LPCWSTR udp_port, LPCWSTR svr_ip_addr);
--					void setup_svr_addr(SOCKADDR_IN* svr_addr, LPCWSTR tcp_port, LPCWSTR svr_ip_addr);
--					void send_request(int type, LPCWSTR request);
--					void terminate_client();
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--					March 24, 2019 - JK - Added FTP Feature
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

LPCWSTR tcp_port_num;
LPCWSTR udp_port_num;
LPCWSTR voip_send_udp_port_num;
LPCWSTR voip_receive_udp_port_num;
std::wstring current_device_ip;

SOCKADDR_IN cl_addr;
SOCKADDR_IN server_addr_tcp;
SOCKADDR_IN server_addr_udp;
SOCKADDR_IN server_addr_voip_send_udp;
SOCKADDR_IN voip_svr_addr_udp;
int server_len;

HANDLE FileReceiverThread;
HANDLE FileStreamerThread;
FTP_INFO ftp_info;

std::vector<std::string> client_msgs;

DWORD clientThreads[20];
int cl_threadCount;

BOOL isConnected = TRUE;

WSAEVENT FtpCompleted;
WSAEVENT FileStreamCompleted;

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
	//open udp socket
	udp_port_num = udp_port;
	initialize_wsa(udp_port, &cl_addr);
	open_socket(&cl_udp_audio_socket, SOCK_DGRAM, IPPROTO_UDP);

	//open voip send socket
	//KTODO: remove port number hardcoding
	voip_send_udp_port_num = L"4981";
	initialize_wsa(voip_send_udp_port_num, &cl_addr);
	open_socket(&cl_udp_voip_send_socket, SOCK_DGRAM, IPPROTO_UDP);
	setup_svr_addr(&voip_svr_addr_udp, voip_send_udp_port_num, svr_ip_addr);

	//open voip receive socket
	voip_receive_udp_port_num = L"4982";
	initialize_wsa(voip_receive_udp_port_num, &cl_addr);
	open_socket(&cl_udp_voip_receive_socket, SOCK_DGRAM, IPPROTO_UDP);
	/*setup_svr_addr(&cl_addr, voip_receive_udp_port_num, current_device_ip.c_str());*/


	if (bind(cl_udp_voip_receive_socket, (struct sockaddr *)&cl_addr, sizeof(sockaddr)) == SOCKET_ERROR) {
		update_status(disconnectedMsg);
		update_client_msgs("Failed to bind udp socket " + std::to_string(WSAGetLastError()));
	}

	//set REUSEADDR for udp socket
	if (setsockopt(cl_udp_voip_receive_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}

	//open tcp socket 
	tcp_port_num = tcp_port;
	initialize_wsa(tcp_port, &cl_addr);
	open_socket(&cl_tcp_req_socket, SOCK_STREAM, IPPROTO_TCP);

	current_device_ip = get_device_ip();
	setup_svr_addr(&server_addr_tcp, tcp_port, svr_ip_addr);
	// TODO: make current dev IP consistent with hardcode localhost
	// This is for client receiver
	setup_svr_addr(&server_addr_udp, udp_port, current_device_ip.c_str());
	// This is to send data to the voip server
	setup_svr_addr(&server_addr_voip_send_udp, voip_send_udp_port_num, svr_ip_addr);

	if (connect(cl_tcp_req_socket, (struct sockaddr *)&server_addr_tcp, sizeof(sockaddr)) == -1)
	{
		isConnected = FALSE;

		update_status(disconnectedMsg);
		update_client_msgs("Failed to connect to server " + std::to_string(WSAGetLastError()));
	}

	if (bind(cl_udp_audio_socket, (struct sockaddr *)&server_addr_udp, sizeof(sockaddr)) == SOCKET_ERROR) {
		isConnected = FALSE;

		update_status(disconnectedMsg);
		update_client_msgs("Failed to bind udp socket " + std::to_string(WSAGetLastError()));
	}

	if (setsockopt(cl_udp_audio_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}

	if (setsockopt(cl_udp_voip_send_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&bOptVal, bOptLen) == SOCKET_ERROR) {
		update_client_msgs("Failed to set reuseaddr with error " + std::to_string(WSAGetLastError()));
	}

	WAVEFORMATEX wfx_fs_play;
	wfx_fs_play.nSamplesPerSec = 44100; /* sample rate */
	wfx_fs_play.wBitsPerSample = 16; /* sample size */
	wfx_fs_play.nChannels = 2; /* channels*/
	wfx_fs_play.cbSize = 0; /* size of _extra_ info */
	wfx_fs_play.wFormatTag = WAVE_FORMAT_PCM;
	wfx_fs_play.nBlockAlign = (wfx_fs_play.wBitsPerSample * wfx_fs_play.nChannels) >> 3;
	wfx_fs_play.nAvgBytesPerSec = wfx_fs_play.nBlockAlign * wfx_fs_play.nSamplesPerSec;

	initialize_waveout_device(wfx_fs_play);

	if (isConnected) {
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
--	FUNCTION:	send_request
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void send_request(int type, LPCWSTR request)
--									int type - the type of request
--									LPCWSTR request - the request message
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to send a request to the server
--------------------------------------------------------------------------------------*/
void send_request(int type, LPCWSTR request)
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
void request_wav_file(LPCWSTR filename) {

	DWORD ThreadId;
	initialize_wsa_events(&FtpCompleted);

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

	send_request(WAV_FILE_REQUEST_TYPE, filename);
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

	send_request(AUDIO_STREAM_REQUEST_TYPE, stream_req_msg);
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
	LPCWSTR receiving_port = L"4981";
	LPCWSTR sending_port = L"4982";
	LPCWSTR ip_addr = L"127.0.0.1";

	WSAEVENT VoipCompleted;
	initialize_wsa_events(&VoipCompleted);

	initialize_voip(&cl_udp_voip_receive_socket, &cl_udp_voip_send_socket, &server_addr_voip_send_udp, VoipCompleted, NULL);

	// send request to voip
	update_client_msgs("Requesting VOIP from server...");
	// Make up a message to pass device ip address into request
	std::wstring temp_str = L"voip";
	std::wstring temp_msg = temp_str + packetMsgDelimiter + current_device_ip + packetMsgDelimiter + receiving_port + packetMsgDelimiter;
	LPCWSTR stream_req_msg = temp_msg.c_str();
	send_request(VOIP_REQUEST_TYPE, stream_req_msg);

	WAVEFORMATEX wfx_voip_play;
	wfx_voip_play.nSamplesPerSec = 11025; /* sample rate */
	wfx_voip_play.wBitsPerSample = 8; /* sample size */
	wfx_voip_play.nChannels = 2; /* channels*/
	wfx_voip_play.cbSize = 0; /* size of _extra_ info */
	wfx_voip_play.wFormatTag = WAVE_FORMAT_PCM;
	wfx_voip_play.nBlockAlign = (wfx_voip_play.wBitsPerSample * wfx_voip_play.nChannels) >> 3;
	wfx_voip_play.nAvgBytesPerSec = wfx_voip_play.nBlockAlign * wfx_voip_play.nSamplesPerSec;

	initialize_waveout_device(wfx_voip_play);


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
	if (client_msgs.size() >= 6) {
		client_msgs.erase(client_msgs.begin());
	}
	client_msgs.push_back(cur_time + message);
	update_messages(client_msgs);
}