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
void start_file_stream(std::string filename, std::string client_port_num, std::string client_ip_addr);
void start_voip();
void terminate_server();
void update_server_msgs(std::string message);
void setup_client_addr(SOCKADDR_IN* client_addr, std::string client_port, std::string client_ip_addr);
void resume_streaming();