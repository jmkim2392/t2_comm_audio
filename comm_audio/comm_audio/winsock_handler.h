#pragma once

#include <stdlib.h>
#include <string>
#include <sstream>
#include <fstream>
#include "main.h"

typedef struct _CLIENT_THREAD_PARAMS {
	LPCWSTR ip_address;
	LPCWSTR packet_size;
	LPCWSTR number_packets;
} CLIENT_THREAD_PARAMS, *LPCLIENT_THREAD_PARAMS;

typedef struct _SOCKET_INFORMATION SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

static SOCKET HostSocket;
static WORD wVersionRequested = MAKEWORD(2, 2);
static int pSize;
static int pNum;
static LPCWSTR connection_mode;

void initialize_wsa(LPCWSTR protocol, LPCWSTR port_number);
void terminate_connection();