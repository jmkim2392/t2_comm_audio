#pragma once

#include "main.h"

#define RECEIVING_PORT 4981
#define SEND_PORT 4982

#define MAXLEN 64

DWORD WINAPI ReceiverThreadFunc(LPVOID lpParameter);
DWORD WINAPI SenderThreadFunc(LPVOID lpParameter);