#pragma once

#include "main.h"
#include <ws2tcpip.h>

#define MAXADDRSTR				16
#define TIMECAST_ADDR			"234.5.6.7"
#define TIMECAST_PORT			8910
#define TIMECAST_TTL			2

bool init_winsock(WSADATA *stWSAData);
bool get_datagram_socket(SOCKET *hSocket);
bool bind_socket(SOCKADDR_IN *stLclAddr, SOCKET *hSocket, u_short nPort);
bool join_multicast_group(struct ip_mreq *stMreq, SOCKET *hSocket, char *achMCAddr);
bool set_ip_ttl(SOCKET *hSocket, u_long  lTTL);
bool disable_loopback(SOCKET *hSocket);
bool set_socket_option_reuseaddr(SOCKET *hSocket);
bool leave_multicast_group(struct ip_mreq *stMreq, SOCKET *hSocket, char *achMCAddr);

DWORD WINAPI receive_data(LPVOID lp);
void CALLBACK multicast_receive_audio(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI broadcast_data(LPVOID lp);
void stop_broadcast();