#pragma once
#include "main.h"

void initialize_wsa_events(WSAEVENT* eventToInit);
void initialize_events_gen(HANDLE* eventToInit, LPCWSTR eventName);
void add_new_thread_gen(DWORD threadList[], DWORD threadId, int threadCount);
void TriggerWSAEvent(WSAEVENT event);
void TriggerEvent(HANDLE event);
std::string get_current_time();