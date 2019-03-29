#pragma once
#include "main.h"
#include "winsock_handler.h"
#include "request_handler.h"
#include "ftp_handler.h"

typedef struct _TCP_SOCKET_INFO TCP_SOCKET_INFO, *LPTCP_SOCKET_INFO;
typedef struct _BROADCAST_INFO BROADCAST_INFO, *LPBROADCAST_INFO;

void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port);
void start_request_receiver();
void start_request_handler();
void start_broadcast(SOCKET* socket, LPCWSTR udp_port);
void initialize_events();
DWORD WINAPI connection_monitor(LPVOID tcp_socket);
DWORD WINAPI broadcast_audio(LPVOID broadcastInfo);
void add_new_thread(DWORD threadId);
void start_ftp(std::string filename);
void terminate_server();
void update_server_msgs(std::string message);