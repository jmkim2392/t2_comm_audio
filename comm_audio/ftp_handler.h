#pragma once
#include "main.h"

void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags);

void initialize_ftp(SOCKET* socket);
void read_file(std::string filename);
void create_new_file(std::string filename);
void write_file();
void packetize_file();
void start_sending_file();
void receive_file();