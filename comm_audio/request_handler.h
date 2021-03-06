#pragma once
#include "winsock_handler.h"
#include "main.h"
#include "voip_handler.h"
#include <string>

typedef struct _TCP_SOCKET_INFO {
	WSAEVENT event;
	SOCKET tcp_socket;
	WSAEVENT CompleteEvent;
	WSAEVENT FtpCompleteEvent;
} TCP_SOCKET_INFO, *LPTCP_SOCKET_INFO;

typedef struct _REQUEST_PACKET {
	int type;
	std::string message;
	std::string ip_addr;
	std::string port_num;
	std::vector<std::string> file_list;
} REQUEST_PACKET, *LPREQUEST_PACKET;

DWORD WINAPI SvrRequestReceiverThreadFunc(LPVOID lpParameter);
DWORD WINAPI ClntReqReceiverThreadFunc(LPVOID lpParameter);
void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI HandleRequest(LPVOID lpParameter);
void parseRequest(LPREQUEST_PACKET parsedPacket, std::string packet);
std::string generateRequestPacket(int type, std::string message);
void start_receiving_requests(SOCKET request_socket, WSAEVENT recvReqEvent, WSAEVENT ftpCompleteEvent);
void parseFileListRequest(LPREQUEST_PACKET parsedPacket, std::string packet);
std::string generateReqPacketWithData(int type, std::vector<std::string> messages);
void getPacketType(LPREQUEST_PACKET parsedPacket, std::string packet);
void terminateRequestHandler();