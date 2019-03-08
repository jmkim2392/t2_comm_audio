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

static HINSTANCE hInstance;
static WNDCLASSEX Wcl;

typedef struct _SOCKET_INFORMATION {
	OVERLAPPED Overlapped;
	SOCKET Socket;
	CHAR Buffer[64];
	WSABUF DataBuf;
	WSAEVENT CompletedEvent;
	DWORD BytesSEND;
	DWORD BytesRECV;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

static HWND parent_hwnd, child_hwnd;

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance,
	LPSTR lspszCmdParam, int nCmdShow);

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message,
	WPARAM wParam, LPARAM lParam);

void InitializeWindow(HINSTANCE hInst, int nCmdShow);