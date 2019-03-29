#pragma once
#include "main.h"

void initialize_events_gen(WSAEVENT* eventToInit);
void add_new_thread_gen(DWORD threadList[], DWORD threadId, int threadCount);
std::string get_current_time();