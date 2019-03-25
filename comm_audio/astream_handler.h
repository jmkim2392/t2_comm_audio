#pragma once
#include "main.h"

typedef struct _ASTREAM_INFO {
	WSAEVENT AStreamCompleteEvent;
	LPCWSTR filename;
} ASTREAM_INFO, *LPASTREAM_INFO;

void initialize_astream(SOCKET* socket, WSAEVENT astreamCompletedEvent);
DWORD WINAPI ReceiveAStreamThreadFunc(LPVOID lpParameter);

