#include "server_handler.h"

//list of threadids
DWORD ThreadList[MAX_THREADS];
HANDLE hThreadList[MAX_THREADS];

void initialize_server() {
	//initiazlie wsa
	// start connection monitor
	// start request handler
	if ((ThreadHandle = CreateThread(NULL, 0, RequestProcessRoutine, (LPVOID)AcceptEvent, 0, &ThreadId)) == NULL)
	{
	}
	// start broadcast TBD

	// wait for terminate signal from windows to start tear down process 
	//WaitForSingleObject();
}

void terminate_server() {

	int array_length = sizeof(hThreadList) / sizeof(HANDLE);

	WaitForMultipleObjects(array_length, hThreadList, TRUE, INFINITE);
	for (int i = 0; i < array_length; i++)
	{
		CloseHandle(hThreadList[i]);
		//clean up any memory used for data if any
	}
}