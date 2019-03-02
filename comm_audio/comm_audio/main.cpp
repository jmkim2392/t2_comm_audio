/*-------------------------------------------------------------------------------------
--	SOURCE FILE: main.cpp - Main src file handles various menu functions and windows
--							messages.
--
--	PROGRAM:		Winsock2_API_Interface
--
--	FUNCTIONS:
--					int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance,
--											LPSTR lspszCmdParam, int nCmdShow)
--					LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--					LRESULT CALLBACK GetInputProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
--					void initialize_dialog(HWND hwnd, LPCWSTR type)
--					void get_user_input(LPCWSTR type, LPCWSTR prompt)
--
--	DATE:			March 2, 2019
--
--	REVISIONS:		March 2, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	NOTES:
--	This program allows users utilize winsock2 APIs through a GUI.
--	Users can obtain host name, IP addresses, service, and port number.
--
--------------------------------------------------------------------------------------*/
#define STRICT
#include "main.h"

LPCWSTR mode = new TCHAR[MAX_INPUT_LENGTH];
//LPCLIENT_THREAD_PARAMS ClientThreadParams;

std::vector<std::wstring> messages;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	WinMain
--
--	DATE:			March 2, 2019
--
--	REVISIONS:		March 2, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance,
--									LPSTR lspszCmdParam, int nCmdShow)
--
--	RETURNS:		int
--
--	NOTES:
--	Call this function to create the main window and monitor events received from user
--------------------------------------------------------------------------------------*/
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance, LPSTR lspszCmdParam, int nCmdShow)
{
	MSG Msg;

	InitializeWindow(hInst, nCmdShow);

	//initialize both sockets
	//initialize the worker threads to return early if error happens

	while (GetMessage(&Msg, NULL, 0, 0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	return Msg.wParam;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	WndProc
--
--	DATE:			March 2, 2019
--
--	REVISIONS:		March 2, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	Call this to handle window messages for main window.
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT paintstruct;
	static int y = 0;

	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_CONNECT:
			//test with just as a server
			break;
		case IDM_EXIT:
			PostQuitMessage(0);
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &paintstruct);
		if (messages.size() != 0) {
			for (auto msg : messages)
			{
				TextOutW(hdc, 0, y, msg.c_str(), msg.length());
				y += 20;
			}
		}
		y = 0;
		EndPaint(hwnd, &paintstruct);
		ReleaseDC(hwnd, hdc);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}

void InitializeWindow(HINSTANCE hInst, int nCmdShow) {
	Wcl.cbSize = sizeof(WNDCLASSEX);
	Wcl.style = CS_HREDRAW | CS_VREDRAW;
	Wcl.hIcon = LoadIcon(NULL, IDI_APPLICATION); // large icon 
	Wcl.hIconSm = NULL; // use small version of large icon
	Wcl.hCursor = LoadCursor(NULL, IDC_ARROW);  // cursor style

	Wcl.lpfnWndProc = WndProc;
	Wcl.hInstance = hInst;
	Wcl.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); //white background
	Wcl.lpszClassName = Name;

	Wcl.lpszMenuName = MenuName; // The menu Class
	Wcl.cbClsExtra = 0;      // no extra memory needed
	Wcl.cbWndExtra = 0;

	if (!RegisterClassEx(&Wcl))
		return ;

	parent_hwnd = CreateWindow(Name, Name, WS_OVERLAPPEDWINDOW, 10, 10, 600, 400, NULL, NULL, hInst, NULL);
	ShowWindow(parent_hwnd, nCmdShow);
	UpdateWindow(parent_hwnd);
	return;
}