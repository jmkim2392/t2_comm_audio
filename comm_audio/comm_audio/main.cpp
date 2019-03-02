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
--	DATE:			January 21, 2019
--
--	REVISIONS:		January 21, 2019
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
--	DATE:			January 21, 2019
--
--	REVISIONS:		January 21, 2019
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
	size_t i;

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
--	DATE:			January 21, 2019
--
--	REVISIONS:		January 21, 2019
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
			//create_dialog_box(Connect_text);
			//test with just as a server
			break;
		case IDM_SETTINGS:
			//create_dialog_box(Settings_text);
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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	GetInputProc
--
--	DATE:			January 21, 2019
--
--	REVISIONS:		January 21, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK GetInputProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	Call this to handle window messages for input dialog.
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK GetInputProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	LPWSTR type = new TCHAR[MAX_INPUT_LENGTH];

	std::vector<std::wstring> api_messages;

	LPWSTR mode_input = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR port_number = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR protocol_input = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR ip_address_input = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR packet_size_input = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR number_packets_input = new TCHAR[MAX_INPUT_LENGTH];

	memset(type, 0, MAX_INPUT_LENGTH);
	memset(mode_input, 0, MAX_INPUT_LENGTH);
	memset(port_number, 0, MAX_INPUT_LENGTH);
	memset(protocol_input, 0, MAX_INPUT_LENGTH);
	memset(ip_address_input, 0, MAX_INPUT_LENGTH);
	memset(packet_size_input, 0, MAX_INPUT_LENGTH);
	memset(number_packets_input, 0, MAX_INPUT_LENGTH);

	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			/*GetWindowText(hwnd, type, MAX_INPUT_LENGTH);
			messages.clear();
			if (wcscmp(type, Connect_text) == 0)
			{
				GetDlgItemText(hwnd, IDC_FIRST_INPUT, port_number, MAX_INPUT_LENGTH);
				if (wcslen(port_number) == 0)
				{
					MessageBox(hwnd, L"You must enter a port number", L"", MB_OK);
				}
				else
				{
					GetDlgItemText(hwnd, IDC_OPTIONS, mode_input, MAX_INPUT_LENGTH);
					mode = mode_input;
					GetDlgItemText(hwnd, IDC_SECOND_INPUT, ip_address_input, MAX_INPUT_LENGTH);

					if (wcscmp(mode_input, Client_text) == 0 && wcslen(ip_address_input) == 0)
					{
						MessageBox(hwnd, L"You must enter an IP Address", L"", MB_OK);
					}
					else
					{
						ClientThreadParams->ip_address = ip_address_input;
						GetDlgItemText(hwnd, IDC_OPTIONS2, protocol_input, MAX_INPUT_LENGTH);
						reset_printer(mode_input);
						update_default_report(mode_input, protocol_input, ClientThreadParams->packet_size, ClientThreadParams->number_packets);
						initialize_connection(mode_input, protocol_input, port_number, ClientThreadParams);
					}
				}
			}
			else if (wcscmp(type, Settings_text) == 0)
			{
				GetDlgItemText(hwnd, IDC_FIRST_INPUT, packet_size_input, MAX_INPUT_LENGTH);
				GetDlgItemText(hwnd, IDC_SECOND_INPUT, number_packets_input, MAX_INPUT_LENGTH);
				ClientThreadParams->packet_size = packet_size_input;
				ClientThreadParams->number_packets = number_packets_input;
				update_default_report(NULL, NULL, ClientThreadParams->packet_size, ClientThreadParams->number_packets);
			}
			update_window();
			delete type;
			delete port_number;
			delete mode_input;
			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);*/
			break;
		case IDCANCEL:
			delete type;
			delete port_number;
			delete mode_input;
			delete protocol_input;
			delete number_packets_input;
			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			return 1;
		}
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