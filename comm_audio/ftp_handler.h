#pragma once
#include "main.h"

void initialize_ftp(SOCKET* socket, WSAEVENT ftp_packet_recved);
void read_file(std::string filename);
void create_new_file(std::string filename);
void write_file(std::string data);
void packetize_file();
void start_sending_file();
void start_receiving_file(int type, LPCWSTR request);
DWORD WINAPI ReceiveFile(LPVOID lpParameter);
DWORD WINAPI ReceiveFileThreadFunc(LPVOID lpParameter);
void CALLBACK FTP_ReceiveRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags);