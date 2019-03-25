#pragma once
#include "main.h"
#include "general_functions.h"

typedef struct _REQUEST_PACKET REQUEST_PACKET, *LPREQUEST_PACKET;

void initialize_client(LPCWSTR tcp_port, LPCWSTR udp_port, LPCWSTR svr_ip_addr);
void setup_svr_addr(SOCKADDR_IN* svr_addr, LPCWSTR tcp_port, LPCWSTR svr_ip_addr);
void send_request(int type, LPCWSTR request);
void request_wav_file(LPCWSTR filename);
void terminate_client();