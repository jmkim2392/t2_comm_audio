#pragma once

#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define DEFAULT_PORT 4985
#define MAX_INPUT_LENGTH 64

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <time.h>
#include <sstream>
#include <locale>
#include <codecvt>
#include <atlstr.h>
#include "constants.h"
#include "server.h"
#include "circular_buffer.h"
#include "client.h"
#include "gui_resource.h"
#include "audio_api.h"

static HINSTANCE hInstance;
static WNDCLASSEX Wcl;

typedef struct _SOCKET_INFORMATION {
	OVERLAPPED Overlapped;
	SOCKET Socket;
	CHAR Buffer[DEFAULT_REQUEST_PACKET_SIZE];
	CHAR FTP_BUFFER[FTP_PACKET_SIZE];
	CHAR AUDIO_BUFFER[AUDIO_BLOCK_SIZE];
	CHAR VOIP_RECV_BUFFER[VOIP_BLOCK_SIZE];
	WSABUF DataBuf;
	WSAEVENT CompletedEvent;
	WSAEVENT FtpCompletedEvent;
	HANDLE EventTrigger;
	SOCKADDR_IN Sock_addr;
	DWORD BytesSEND;
	DWORD BytesRECV;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

typedef struct _BROADCAST_INFO {
	SOCKET udpSocket;
	int portNum;
} BROADCAST_INFO, *LPBROADCAST_INFO;

typedef struct _VOIP_INFORMATION {
	WSAEVENT CompletedEvent;
	LPCWSTR Ip_addr;
	LPCWSTR Udp_Port;
} VOIP_INFO, *LPVOIP_INFO;

static HWND parent_hwnd, child_hwnd, control_panel_hwnd, popup;

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance, LPSTR lspszCmdParam, int nCmdShow);

void InitializeWindow(HINSTANCE hInst, int nCmdShow);

void show_dialog(int type, HWND p_hwnd);

void show_control_panel(int type);

void enableButtons(bool isOn);

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ServerDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ClientDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ServerControlPanelProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ClientControlPanelProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK FileReqProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StreamProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);


void update_status(std::string newStatus);

void update_messages(std::vector<std::string> messages);

void start_Server_Stream();
void setup_file_list_dropdown(std::vector<std::string> options);
void close_popup();
