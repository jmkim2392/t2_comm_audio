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

SOCKADDR_IN cl_addr;
SOCKADDR_IN server_addr;

HANDLE FileReceiverThread;
HANDLE AStreamReceiverThread;
FTP_INFO ftp_info;
ASTREAM_INFO astream_info;

DWORD clientThreads[20];
int cl_threadCount;

BOOL isConnected = FALSE;

WSAEVENT FtpCompleted;
WSAEVENT AStreamCompleted;

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
	//open udp socket
	initialize_wsa(udp_port, &cl_addr);
	open_socket(&cl_udp_audio_socket, SOCK_DGRAM, IPPROTO_UDP);

	//open tcp socket 
	initialize_wsa(tcp_port, &cl_addr);
	open_socket(&cl_tcp_req_socket, SOCK_STREAM, IPPROTO_TCP);

	setup_svr_addr(&server_addr, tcp_port, svr_ip_addr);

	if (connect(cl_tcp_req_socket, (struct sockaddr *)&server_addr, sizeof(sockaddr)) == -1)
	{
		fprintf(stderr, "Can't connect to server\n");
		isConnected = FALSE;
		perror("connect");
		exit(1);
	}

	isConnected = TRUE;

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
		fprintf(stderr, "Unknown server address\n");
		exit(1);
	}

	// Copy the server address
	memcpy((char *)&svr_addr->sin_addr, hp->h_addr, hp->h_length);
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
			printf("WSASend() failed with error %d\n", WSAGetLastError());
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
	initialize_events_gen(&FtpCompleted);

	initialize_ftp(&cl_tcp_req_socket, FtpCompleted);

	ftp_info.filename = filename;
	ftp_info.FtpCompleteEvent = FtpCompleted;

	create_new_file("receivedWav.wav");

	if ((FileReceiverThread = CreateThread(NULL, 0, ReceiveFileThreadFunc, (LPVOID)&ftp_info, 0, &ThreadId)) == NULL)
	{
		printf("CreateThread failed with error %d\n", GetLastError());
		return;
	}

	add_new_thread_gen(clientThreads, ThreadId, cl_threadCount++);

	send_request(WAV_FILE_REQUEST_TYPE, filename);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	request_astream
--
--	DATE:			March 24, 2019
--
--	REVISIONS:		March 24, 2019
--
--	DESIGNER:		Keishi Asai
--
--	PROGRAMMER:		Keishi Asai
--
--	INTERFACE:		void request_wav_file(LPCWSTR filename)
--									LPCWSTR filename - the name of file to request from server
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to request a wav file from server
--------------------------------------------------------------------------------------*/
void request_astream(LPCWSTR filename) 
{
	// UDP are already listening here at 4986
	DWORD ThreadId;
	initialize_events_gen(&AStreamCompleted);

	// This should be used for request
	// TODO: FTP's this init function call taking complex include path I guess
	// like client.h -> main.h -> server.h -> ftp_handler.h
	initialize_astream(&cl_tcp_req_socket, AStreamCompleted);

	astream_info.filename = filename;
	astream_info.AStreamCompleteEvent = AStreamCompleted;

	if ((AStreamReceiverThread = CreateThread(NULL, 0, ReceiveAStreamThreadFunc, (LPVOID)&astream_info, 0, &ThreadId)) == NULL)
	{
		printf("CreateThread failed with error %d\n", GetLastError());
		return;
	}

	add_new_thread_gen(clientThreads, ThreadId, cl_threadCount++);

	send_request(AUDIO_STREAM_REQUEST_TYPE, filename);

	// TODO: keishi in ftp_handler, FTP_SendRoutine is a good example of sending file with CR and TCP
	// should be able to modify it for just UDP
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