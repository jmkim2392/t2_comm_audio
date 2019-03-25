#pragma once
#include "main.h"

typedef struct _PSTREAM_INFO {
	WSAEVENT PStreamCompleteEvent;
	LPCWSTR filename;
} PSTREAM_INFO, *LPPSTREAM_INFO;

void initialize_pstream(SOCKET* socket, WSAEVENT pstreamCompletedEvent);
DWORD WINAPI ReceivePStreamThreadFunc(LPVOID lpParameter);

