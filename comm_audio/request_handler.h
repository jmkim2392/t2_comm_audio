#pragma once
#include "winsock_handler.h"
#include "main.h"
#include <string>

typedef struct _TCP_SOCKET_INFO {
	WSAEVENT event;
	SOCKET tcp_socket;
	WSAEVENT CompleteEvent;
} TCP_SOCKET_INFO, *LPTCP_SOCKET_INFO;

typedef struct _REQUEST_PACKET {
	int type;
	std::string message;
} REQUEST_PACKET, *LPREQUEST_PACKET;

DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter);
void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI HandleRequest(LPVOID lpParameter);
void parseRequest(LPREQUEST_PACKET parsedPacket, std::string packet);
void TriggerEvent(WSAEVENT event);
std::string generateRequestPacket(int type, std::string message);	