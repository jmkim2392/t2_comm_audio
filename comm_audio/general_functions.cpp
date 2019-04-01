#include "general_functions.h"

/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_events
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
--	Call this function to intialize an event for synchronization
--------------------------------------------------------------------------------------*/
void initialize_events_gen(WSAEVENT* eventToInit)
{
	if ((*eventToInit = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		return;
	}
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

std::string get_current_time() {
	char str[70];
	struct tm buf;
	time_t cur_time = time(nullptr);
	localtime_s(&buf, &cur_time);
	strftime(str, 100, "%Y-%m-%d %X - ", &buf);
	std::string time_str(str);
	return time_str;
}