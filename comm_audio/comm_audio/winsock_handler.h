#pragma once

#include <stdlib.h>
#include <string>
#include <sstream>
#include <fstream>
#include "main.h"

typedef struct _SOCKET_INFORMATION SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

static WORD wVersionRequested = MAKEWORD(2, 2);
static int pSize;
static int pNum;
static LPCWSTR connection_mode;

void initialize_wsa(LPCWSTR port_number);
void open_socket(SOCKET* socket, int socket_type, int protocol_type);
void terminate_connection();