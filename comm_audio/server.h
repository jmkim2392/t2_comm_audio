#pragma once
#include <filesystem>
#include "main.h"
#include "winsock_handler.h"
#include "request_handler.h"
#include "ftp_handler.h"
#include "multicast.h"
#include "audio_api.h"

typedef struct _TCP_SOCKET_INFO TCP_SOCKET_INFO, *LPTCP_SOCKET_INFO;
typedef struct _BROADCAST_INFO BROADCAST_INFO, *LPBROADCAST_INFO;

void initialize_server(LPCWSTR tcp_port, LPCWSTR udp_port);
void start_request_receiver();
void start_request_handler();
void start_broadcast();
DWORD WINAPI connection_monitor(LPVOID tcp_socket);
void start_ftp(std::string filename, std::string ip_addr);
void start_file_stream(std::string filename, std::string client_port_num, std::string client_ip_addr);
void start_voip(std::string client_port_num, std::string client_ip_addr);
void terminate_server();
void update_server_msgs(std::string message);
void setup_client_addr(SOCKADDR_IN* client_addr, std::string client_port, std::string client_ip_addr);
void resume_streaming();
void send_request_to_clnt(std::string msg);
std::vector<std::string> get_file_list();