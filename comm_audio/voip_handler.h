#pragma once

#include "main.h"

#define RECEIVING_PORT 4981
#define SEND_PORT 4982

#define MAXLEN 64

DWORD WINAPI ReceiverThreadFunc(LPVOID lpParameter);
DWORD WINAPI SenderThreadFunc(LPVOID lpParameter);
void start_receiving_voip(LPCWSTR ip_addr, LPCWSTR udp_port);
void CALLBACK Voip_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void closeVOIP();