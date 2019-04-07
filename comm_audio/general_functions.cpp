/*-------------------------------------------------------------------------------------
--	SOURCE FILE: general_functions.cpp - Contains generic functions for comm_audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_wsa_events(WSAEVENT* eventToInit);
--					void initialize_events_gen(HANDLE* eventToInit, LPCWSTR eventName);
--					void add_new_thread_gen(DWORD threadList[], DWORD threadId, int threadCount);
--					void TriggerWSAEvent(WSAEVENT event);
--					void TriggerEvent(HANDLE event);
--					std::string get_current_time();
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--					March 31, 2019 - added event functions for non wsa events
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--
--------------------------------------------------------------------------------------*/
#include "general_functions.h"

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_wsa_events
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_events(WSAEVENT* eventToInit)
--									WSAEVENT* - the event to initialize
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize a WSA event for synchronization
--------------------------------------------------------------------------------------*/
void initialize_wsa_events(WSAEVENT* eventToInit)
{
	if ((*eventToInit = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		return;
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_events
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void initialize_events(HANDLE* eventToInit, LPCWSTR eventName)
--									HANDLE* eventToInit - the event to initialize
--									LPCWSTR eventName - name of the event
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to intialize an event for synchronization
--------------------------------------------------------------------------------------*/
void initialize_events_gen(HANDLE* eventToInit, LPCWSTR eventName)
{
	*eventToInit = CreateEventW(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is nonsignaled
		eventName  // object name
	);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	TriggerWSAEvent
--
--	DATE:			March 8, 2019
--
--	REVISIONS:		March 8, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void TriggerWSAEvent(WSAEVENT event)
--									WSAEVENT event - event to trigger
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to trigger a WSAevent
--------------------------------------------------------------------------------------*/
void TriggerWSAEvent(WSAEVENT event) 
{
	if (WSASetEvent(event) == FALSE)
	{
		update_server_msgs("WSASetEvent failed in TriggerWSAEvent with error " + std::to_string(WSAGetLastError()));
		return;
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	TriggerEvent
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void TriggerEvent(HANDLE event)
--									HANDLE event - event to trigger
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to trigger an event
--------------------------------------------------------------------------------------*/
void TriggerEvent(HANDLE event)
{
	SetEvent(event);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	add_new_thread
--
--	DATE:			March 14, 2019
--
--	REVISIONS:		March 14, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void add_new_thread(DWORD threadList[], DWORD threadId, int threadCount)
--								DWORD threadList[] - the list to add theadIds
--								DWORD threadId - the threadId to add
--								int threadCount - the number of threads
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to add a new thread to maintain the list of active threads
--------------------------------------------------------------------------------------*/
void add_new_thread_gen(std::vector<HANDLE> threadList, HANDLE &thrd)
{
	threadList.push_back(thrd);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	get_current_time
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		APril 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		std::string get_current_time() 
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to get the current timestamp
--------------------------------------------------------------------------------------*/
std::string get_current_time() 
{
	char str[70];
	struct tm buf;
	time_t cur_time = time(nullptr);
	localtime_s(&buf, &cur_time);
	strftime(str, 100, "%Y-%m-%d %X - ", &buf);
	std::string time_str(str);
	return time_str;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	get_device_ip
--
--	DATE:			April 4, 2019
--
--	REVISIONS:		APril 4, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		std::wstring get_device_ip() 
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to get the current device's first active ip address
--------------------------------------------------------------------------------------*/
std::wstring get_device_ip() 
{
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;

	unsigned int i = 0;

	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

	// default to IPv4 addresses
	ULONG family = AF_INET;

	LPVOID lpMsgBuf = NULL;

	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	ULONG outBufLen = 0;
	ULONG Iterations = 0;

	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
	PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
	PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
	IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
	IP_ADAPTER_PREFIX *pPrefix = NULL;

	// Allocate a 15 KB buffer to start with.
	outBufLen = WORKING_BUFFER_SIZE;
	wchar_t ipstringbuffer[MAX_INPUT_LENGTH+1];
	memset(ipstringbuffer, 0, sizeof(MAX_INPUT_LENGTH+1));
	INT iRetval;

	do {
		pAddresses = (IP_ADAPTER_ADDRESSES *)MALLOC(outBufLen);
		if (pAddresses == NULL) {
			printf
			("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
			exit(1);
		}

		dwRetVal =
			GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			FREE(pAddresses);
			pAddresses = NULL;
		}
		else {
			break;
		}
		Iterations++;
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

	if (dwRetVal == NO_ERROR) {
		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;
		while (pCurrAddresses) {
			pUnicast = pCurrAddresses->FirstUnicastAddress;

			DWORD ipbufferlength = MAX_INPUT_LENGTH+1;

			sockaddr_in* address = (sockaddr_in*)pCurrAddresses->FirstUnicastAddress->Address.lpSockaddr;
			uint32_t ipv4 = address->sin_addr.S_un.S_addr;

			if (pCurrAddresses->OperStatus == IfOperStatusUp) {
				iRetval =WSAAddressToString(pCurrAddresses->FirstUnicastAddress->Address.lpSockaddr, (DWORD)pCurrAddresses->FirstUnicastAddress->Address.iSockaddrLength, NULL, ipstringbuffer, &ipbufferlength);
				std::wstring ws(ipstringbuffer);
				return ws;
			}
			pCurrAddresses = pCurrAddresses->Next;
		}
	}
	if (pAddresses) {
		FREE(pAddresses);
	}
	return NULL;
}
