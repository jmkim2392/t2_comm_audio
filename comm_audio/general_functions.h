#pragma once
#include "main.h"
// Link with Iphlpapi.lib
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "IPHLPAPI.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

void initialize_wsa_events(WSAEVENT* eventToInit);
void initialize_events_gen(HANDLE* eventToInit, LPCWSTR eventName);
void add_new_thread_gen(DWORD threadList[], DWORD threadId, int threadCount);
void TriggerWSAEvent(WSAEVENT event);
void TriggerEvent(HANDLE event);
std::string get_current_time();
std::wstring get_device_ip();