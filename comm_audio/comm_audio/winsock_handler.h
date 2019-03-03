#pragma once

#include <stdlib.h>
#include <string>
#include <sstream>
#include <fstream>
#include "main.h"
#include "worker_routines.h"

typedef struct _SOCKET_INFORMATION SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

static WORD wVersionRequested = MAKEWORD(2, 2);
static int pSize;
static int pNum;
static LPCWSTR connection_mode;

void initialize_wsa(LPCWSTR protocol, LPCWSTR port_number);

// Thread functions
DWORD WINAPI start_connection_monitor(LPVOID tcp_socket);
DWORD WINAPI start_request_handler(LPVOID params);
// Clean up functions
void terminate_connection();