#pragma once
#include "main.h"

typedef struct _FTP_INFO {
	WSAEVENT FtpCompleteEvent;
	LPCWSTR filename;
} FTP_INFO, *LPFTP_INFO;

void initialize_ftp(SOCKET* socket, WSAEVENT ftp_packet_recved);
int open_file(std::string filename);
void create_new_file(std::string filename);
void write_file(char* data, int length);
void start_sending_file();
void send_file_not_found_packet();
void start_receiving_file();
DWORD WINAPI ReceiveFileThreadFunc(LPVOID lpParameter);
void CALLBACK FTP_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void terminateFtpHandler();