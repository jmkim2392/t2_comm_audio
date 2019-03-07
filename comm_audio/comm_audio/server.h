#pragma once
#include "main.h"
#include "winsock_handler.h"
#include "request_handler.h"

void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port);
DWORD WINAPI connection_monitor(LPVOID tcp_socket);
void add_new_thread(DWORD threadId);
void terminate_server();