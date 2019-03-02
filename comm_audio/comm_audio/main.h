#pragma once
#define MAX_INPUT_LENGTH 64
#define WM_WSA_SUCCESS (WM_USER +1)
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_PORT 4985
#define DATA_BUFSIZE 8192
#define BUFSIZE					255
#define DEFAULT_PACKET_SIZE		L"128"
#define DEFAULT_NUM_PACKETS		L"10"

// IP 24.80.222.85
#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include "resource.h"

static HINSTANCE hInstance;
static WNDCLASSEX Wcl;

const LPCWSTR Name = L"Protocol Stress Tester";
const LPCWSTR MenuName = L"SERVICEMENU";
const LPCWSTR InputDialogName = L"INPUTDIALOG";
const LPCWSTR Connect_text = L"Connect";
const LPCWSTR Settings_text = L"Settings";
const LPCWSTR Packet_size_text = L"Packet Size:";
const LPCWSTR Mode_text = L"Mode:";
const LPCWSTR Protocol_text = L"Protocol:";
const LPCWSTR Port_text = L"Port Number:";
const LPCWSTR Num_packets_text = L"Number of Packets:";
const LPCWSTR Client_text = L"Client";
const LPCWSTR Host_text = L"Host";
const LPCWSTR PtS_prompt = L"Enter the port number:";
const std::vector<LPCWSTR> Protocol_types = { L"TCP", L"UDP" };
const std::vector<LPCWSTR> Mode_types = { L"Client", L"Host" };

typedef struct _SOCKET_INFORMATION {
	OVERLAPPED Overlapped;
	SOCKET Socket;
	CHAR Buffer[DATA_BUFSIZE];
	WSABUF DataBuf;
	DWORD BytesSEND;
	DWORD BytesRECV;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

static HWND parent_hwnd, child_hwnd;

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance,
	LPSTR lspszCmdParam, int nCmdShow);

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message,
	WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK GetInputProc(HWND hwnd, UINT Message,
	WPARAM wParam, LPARAM lParam);

void InitializeWindow(HINSTANCE hInst, int nCmdShow);