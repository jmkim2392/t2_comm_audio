#pragma once
#include "winsock_handler.h"
#include "main.h"
#include <string>

#define DEFAULT_REQUEST_PACKET_SIZE 64

DWORD WINAPI RequestHandlerWorkerThread(LPVOID lpParameter);
void CALLBACK RequestHandlerRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);