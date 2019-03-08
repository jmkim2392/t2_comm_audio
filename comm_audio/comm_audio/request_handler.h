#pragma once
#include "winsock_handler.h"
#include "main.h"
#include <string>

#define DEFAULT_REQUEST_PACKET_SIZE 64

typedef struct _REQUEST_HANDLER_INFO {
	WSAEVENT event;
	SOCKET req_sock;
} REQUEST_HANDLER_INFO, *LPREQUEST_HANDLER_INFO;

DWORD WINAPI RequestHandlerWorkerThread(LPVOID lpParameter);
void CALLBACK RequestHandlerRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);