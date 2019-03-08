#pragma once
#include "main.h"
#include "winsock_handler.h"
#include "request_handler.h"

typedef struct _REQUEST_HANDLER_INFO REQUEST_HANDLER_INFO, *LPREQUEST_HANDLER_INFO;

void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port);
void start_request_monitor();
DWORD WINAPI connection_monitor(LPVOID tcp_socket);
void add_new_thread(DWORD threadId);
void terminate_server();