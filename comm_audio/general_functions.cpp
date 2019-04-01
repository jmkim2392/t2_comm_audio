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
		update_server_msgs("WSASetEvent failed in Request Handler with error " + WSAGetLastError());
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
void add_new_thread_gen(DWORD threadList[], DWORD threadId, int threadCount)
{
	threadList[threadCount] = threadId;
}

//std::string get_current_time() {
//
//}