#pragma once
#include "winsock_handler.h"
#include "main.h"
#include <string>

#define DEFAULT_REQUEST_PACKET_SIZE 64

typedef struct _REQUEST_HANDLER_INFO {
	WSAEVENT event;
	SOCKET req_sock;
	WSAEVENT CompleteEvent;
} REQUEST_HANDLER_INFO, *LPREQUEST_HANDLER_INFO;

DWORD WINAPI RequestReceiverThreadFunc(LPVOID lpParameter);
void CALLBACK RequestReceiverRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI HandleRequest(LPVOID lpParameter);
void TriggerEvent(WSAEVENT event);