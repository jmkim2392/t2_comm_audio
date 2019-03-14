#include "general_functions.h"

void initialize_events_gen(WSAEVENT* eventToInit)
{
	if ((*eventToInit = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		printf("WSACreateEvent() failed with error %d\n", WSAGetLastError());
		return;
	}
}

void add_new_thread_gen(DWORD threadList[], DWORD threadId, int threadCount)
{
	threadList[++threadCount] = threadId;
}