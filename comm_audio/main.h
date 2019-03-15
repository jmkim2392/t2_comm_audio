#pragma once
#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define DEFAULT_PORT 4985
#define MAX_INPUT_LENGTH 64

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include "resource.h"
#include "constants.h"
#include "server.h"
#include "circular_buffer.h"
#include "client.h"

static HINSTANCE hInstance;
static WNDCLASSEX Wcl;

typedef struct _SOCKET_INFORMATION {
	OVERLAPPED Overlapped;
	SOCKET Socket;
	CHAR Buffer[DEFAULT_REQUEST_PACKET_SIZE];
	WSABUF DataBuf;
	WSAEVENT CompletedEvent;
	DWORD BytesSEND;
	DWORD BytesRECV;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

typedef struct _BROADCAST_INFO {
	SOCKET udpSocket;
	int portNum;
} BROADCAST_INFO, *LPBROADCAST_INFO;

static HWND parent_hwnd, child_hwnd;

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance,
	LPSTR lspszCmdParam, int nCmdShow);

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message,
	WPARAM wParam, LPARAM lParam);

void InitializeWindow(HINSTANCE hInst, int nCmdShow);