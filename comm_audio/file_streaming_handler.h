#pragma once
#include "main.h"

void initialize_file_stream(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT fileStreamCompletedEvent, HANDLE eventTrigger);
int open_file_to_stream(std::string filename);
void close_file_streaming();
void start_receiving_stream();
DWORD WINAPI ReceiveStreamThreadFunc(LPVOID lpParameter);
void CALLBACK FileStream_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void CALLBACK FileStream_SendRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI SendStreamThreadFunc(LPVOID lpParameter);
void terminateFileStream();