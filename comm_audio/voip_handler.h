#pragma once

#include "main.h"

#define RECEIVING_PORT 4981
#define SEND_PORT 4982

#define MAXLEN 64

DWORD WINAPI ReceiverThreadFunc(LPVOID lpParameter);
void start_receiving_voip();
void CALLBACK Voip_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void send_audio_block(PWAVEHDR whdr);
void initialize_voip(SOCKET* receive_socket, SOCKET* send_socket, SOCKADDR_IN* addr, WSAEVENT voipSendCompletedEvent, HANDLE eventTrigger);
void initialize_voip_receive(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT voipReceiveCompletedEvent, HANDLE eventTrigger);
void initialize_voip_send(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT voipSendCompletedEvent, HANDLE eventTrigger);
