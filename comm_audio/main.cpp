/*-------------------------------------------------------------------------------------
--	SOURCE FILE: main.cpp - Main src file handles various menu functions and windows
--							messages.
--
--	PROGRAM:		Comm_Audio
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
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	NOTES:
--	Temporary Window GUI for network testing/dev. To be updated
--	Currently only implemented with server function
--------------------------------------------------------------------------------------*/
#define STRICT
#include "main.h"

LPCWSTR mode = new TCHAR[MAX_INPUT_LENGTH];
std::vector<std::wstring> messages;
int selectedFeatureType;

/*-------------------------------------------------------------------------------------
--	FUNCTION:	WinMain
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
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

	return (int)Msg.wParam;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	WndProc
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
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
	static int y = 0;

	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_SERVER:
			show_dialog(IDM_SERVER, hwnd);
			break;
		case IDM_CLIENT:
			show_dialog(IDM_CLIENT, hwnd);
			break;
		case IDM_EXIT:
			PostQuitMessage(0);
			break;
		}
		break;
	case WM_PAINT:
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
--	FUNCTION:	InitializeWindow
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void InitializeWindow(HINSTANCE hInst, int nCmdShow)
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize the main program window
--------------------------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------------------------
--	FUNCTION:	show_dialog
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void show_dialog(int type, HWND p_hwnd)
--									int type - the type of dialog to display
--									HWND p_hwnd - the parent window
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to show a dialog box of the specified type
--------------------------------------------------------------------------------------*/
void show_dialog(int type, HWND p_hwnd)
{
	EnableWindow(p_hwnd, FALSE);
	HWND hwndDlg = NULL;

	switch (type)
	{
	case IDM_SERVER:
		popup = CreateDialog(hInstance, ServerDialogName, p_hwnd, (DLGPROC)ServerDialogProc);
		break;
	case IDM_CLIENT:
		popup = CreateDialog(hInstance, ClientDialogName, p_hwnd, (DLGPROC)ClientDialogProc);
		break;
	case IDM_FILE_REQUEST_TYPE:
		popup = CreateDialog(hInstance, FileReqDialogName, p_hwnd, (DLGPROC)FileReqProc);
		send_request_to_svr(FILE_LIST_REQUEST_TYPE, L"FILELISTREQ");
		break;
	case IDM_FILE_STREAM_TYPE:
		popup = CreateDialog(hInstance, FileReqDialogName, p_hwnd, (DLGPROC)FileReqProc);
		send_request_to_svr(FILE_LIST_REQUEST_TYPE, L"FILELISTREQ");
		break;
	case IDM_VOIP_TYPE:
		popup = CreateDialog(hInstance, StreamingDialogName, p_hwnd, (DLGPROC)StreamProc);
		break;
	case IDM_MULTICAST_TYPE:
		popup = CreateDialog(hInstance, StreamingDialogName, p_hwnd, (DLGPROC)StreamProc);
		break;
	}
	ShowWindow(popup, SW_SHOW);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	show_control_panel
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void show_control_panel(int type)
--									int type - Either IDM_SERVER || IDM_CLIENT
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to show a control panel of the specified type
--------------------------------------------------------------------------------------*/
void show_control_panel(int type)
{
	EnableWindow(parent_hwnd, FALSE);

	switch (type)
	{
	case IDM_SERVER:
		control_panel_hwnd = CreateDialog(hInstance, ServerControlPanelName, parent_hwnd, (DLGPROC)ServerControlPanelProc);
		break;
	case IDM_CLIENT:
		control_panel_hwnd = CreateDialog(hInstance, ClientControlPanelName, parent_hwnd, (DLGPROC)ClientControlPanelProc);
		break;
	}

	ShowWindow(control_panel_hwnd, SW_SHOW);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	enableButtons
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void enableButtons(bool isOn) 
--									bool isOn - TRUE to enable, FALSE to disable
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to with 'TRUE' to enable all feature buttons,
--	'FALSE' to disable all feature buttons
--------------------------------------------------------------------------------------*/
void enableButtons(bool isOn) 
{
	int features[4] = { IDM_FILE_REQUEST_TYPE, IDM_FILE_STREAM_TYPE, IDM_VOIP_TYPE, IDM_MULTICAST_TYPE };

	for (int feature : features) 
	{
		EnableWindow(GetDlgItem(control_panel_hwnd, feature), isOn);
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	ServerDialogProc
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK ServerDialogProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	The Server Connection Dialog function to handle the messages
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK ServerDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	LPWSTR tcp_port_num = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR udp_port_num = new TCHAR[MAX_INPUT_LENGTH];

	memset(tcp_port_num, 0, MAX_INPUT_LENGTH);
	memset(udp_port_num, 0, MAX_INPUT_LENGTH);

	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			// get input and initialize server
			GetDlgItemText(hwnd, IDM_TCP_PORT, tcp_port_num, MAX_INPUT_LENGTH);
			GetDlgItemText(hwnd, IDM_UDP_PORT, udp_port_num, MAX_INPUT_LENGTH);

			show_control_panel(IDM_SERVER);

			//TODO: to uncomment after testing features
			initialize_server(tcp_port_num, udp_port_num);

			//TODO: to remove after testing 
			//initialize_server(L"4985", L"4986");

			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			break;
		case IDCANCEL:
			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			return 1;
		}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	ClientDialogProc
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK ClientDialogProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	The Client Connection Dialog function to handle the messages
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK ClientDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	LPWSTR tcp_port_num = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR udp_port_num = new TCHAR[MAX_INPUT_LENGTH];
	LPWSTR server_ip = new TCHAR[MAX_INPUT_LENGTH];

	memset(tcp_port_num, 0, MAX_INPUT_LENGTH);
	memset(udp_port_num, 0, MAX_INPUT_LENGTH);
	memset(server_ip, 0, MAX_INPUT_LENGTH);

	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			// get input and initialize client
			GetDlgItemText(hwnd, IDM_TCP_PORT, tcp_port_num, MAX_INPUT_LENGTH);
			GetDlgItemText(hwnd, IDM_UDP_PORT, udp_port_num, MAX_INPUT_LENGTH);
			GetDlgItemText(hwnd, IDM_IP_ADDRESS, server_ip, MAX_INPUT_LENGTH);


			show_control_panel(IDM_CLIENT);

			//TODO: to uncomment after testing features
			initialize_client(tcp_port_num, udp_port_num, server_ip);

			//TODO: to remove after testing 
			//initialize_client(L"4985", L"4986", L"localhost");

			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			break;
		case IDCANCEL:
			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			return 1;
		}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	ServerControlPanelProc
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK ServerControlPanelProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	THe Server Control Panel Process to handle its messages
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK ServerControlPanelProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
			terminate_server();
			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			return 1;
		}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	ClientControlPanelProc
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK ClientControlPanelProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	The Client Control Panel Process to handle its messages
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK ClientControlPanelProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_FILE_REQUEST_TYPE:
			selectedFeatureType = IDM_FILE_REQUEST_TYPE;
			show_dialog(IDM_FILE_REQUEST_TYPE, hwnd);
			//enableButtons(FALSE);
			break;
		case IDM_FILE_STREAM_TYPE:
			selectedFeatureType = IDM_FILE_STREAM_TYPE;
			show_dialog(IDM_FILE_STREAM_TYPE, hwnd);
			break;
		case IDM_VOIP_TYPE:
			show_dialog(IDM_VOIP_TYPE, hwnd);
			break;
		case IDM_MULTICAST_TYPE:
			show_dialog(IDM_MULTICAST_TYPE, hwnd);
			break;
		case IDCANCEL:
			// Disconnect process
			terminate_client();
			EnableWindow(parent_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			return 1;
		}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	FileReqProc
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK FileReqProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	The File Request Dialog Box Process to handle its messages
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK FileReqProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	LPWSTR filename = new TCHAR[MAX_INPUT_LENGTH];

	memset(filename, 0, MAX_INPUT_LENGTH);

	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			// get the input and initialize filereq/filestream
			GetDlgItemText(hwnd, IDM_FILE_LIST_DROPDOWN, filename, MAX_INPUT_LENGTH);
			
			if (wcslen(filename) == 0)
			{
				update_client_msgs("Incorrect File name.");
			}
			else
			{
				if (selectedFeatureType == IDM_FILE_REQUEST_TYPE)
				{
					//reset_client_request_receiver();
					request_wav_file(filename);
				}
				else
				{
					//reset_client_request_receiver();
					request_file_stream(filename);
					show_dialog(IDM_VOIP_TYPE, control_panel_hwnd);
				}
			}
			reset_client_request_receiver();
			EnableWindow(control_panel_hwnd, TRUE);
			EndDialog(hwnd, wParam);

			break;
		case IDCANCEL:
			reset_client_request_receiver();
			EnableWindow(control_panel_hwnd, TRUE);
			EndDialog(hwnd, wParam);
			return 1;
		}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	StreamProc
--
--	DATE:			March 26, 2019
--
--	REVISIONS:		March 26, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		LRESULT CALLBACK StreamProc(HWND hwnd, UINT Message, WPARAM wParam,
--											LPARAM lParam)
--
--	RETURNS:		LRESULT
--
--	NOTES:
--	The Streaming Dialog Box Process to handle its messages
--------------------------------------------------------------------------------------*/
LRESULT CALLBACK StreamProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
			// Disconnect process
			start_client_terminate_file_stream();
			EnableWindow(control_panel_hwnd, TRUE);
			disconnect_multicast();
			EndDialog(hwnd, wParam);
			return 1;
		}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	update_status
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void update_status(std::string newStatus) 
--									std::string newStatus - the new status message to show
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to change the status message on the control panel
--------------------------------------------------------------------------------------*/
void update_status(std::string newStatus) 
{
	HWND status_message = GetDlgItem(control_panel_hwnd, IDM_STATUS);
	LPWSTR widestr = new WCHAR[newStatus.length() + 1];

	::MultiByteToWideChar(CP_ACP, 0, newStatus.c_str(), (int)newStatus.size(), widestr, (int)newStatus.length());

	widestr[newStatus.length()] = 0;

	SetWindowText(status_message, widestr);
	delete[] widestr;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	update_messages
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void update_messages(std::vector<std::string> messages)  
--									std::vector<std::string> messages - new messages to show
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to output messages on the control panel
--------------------------------------------------------------------------------------*/
void update_messages(std::vector<std::string> messages)  
{
	HWND messageOutput = GetDlgItem(control_panel_hwnd, IDM_FEATURE_MSG);
	LPWSTR output;
	std::string outputString;

	for (auto const& msg : messages)
	{
		outputString += (msg + "\n");
	}
	output = new WCHAR[outputString.length() + 1];
	::MultiByteToWideChar(CP_ACP, 0, outputString.c_str(), (int)outputString.size(), output, (int)outputString.length());

	output[outputString.length()] = 0;

	SetWindowText(messageOutput, output);
	delete[] output;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	setup_file_list_dropdown
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		April 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void setup_file_list_dropdown(std::vector<std::string> options)
--									std::vector<std::string> options - list of options
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to populate the drop down menu
--------------------------------------------------------------------------------------*/
void setup_file_list_dropdown(std::vector<std::string> options)
{
	HWND dropdown = GetDlgItem(popup, IDM_FILE_LIST_DROPDOWN);
	LPWSTR output = new WCHAR[MAX_INPUT_LENGTH];

	for (auto option : options)
	{
		memset(output, 0, MAX_INPUT_LENGTH);
		::MultiByteToWideChar(CP_ACP, 0, option.c_str(), (int)option.size(), output, (int)option.length());
		SendMessage(dropdown, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(output));
	}
	delete[] output;
}

void close_popup()
{
	EndDialog(popup, 0);
}