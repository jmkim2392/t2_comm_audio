#pragma once
#include "main.h"

#define MAX_THREADS 20
#define DATA_BUFSIZE 8192
#define DEFAULT_REQUEST_PACKET_SIZE 64
#define FTP_PACKET_SIZE 4096
#define MAX_INPUT_LENGTH 64

// Request types
#define WAV_FILE_REQUEST_TYPE 1
#define AUDIO_STREAM_REQUEST_TYPE 2
#define VOIP_REQUEST_TYPE 3

// Control Packets
#define FILE_NOT_FOUND 17 // DC1
#define FTP_COMPLETE 18 // DC2

// Window GUI strings
const LPCWSTR Name = L"Comm Audio";
const LPCWSTR MenuName = L"SERVICEMENU";
const LPCWSTR ServerDialogName = L"SERVERDIALOG";
const LPCWSTR ClientDialogName = L"CLIENTDIALOG";
const LPCWSTR ServerControlPanelName = L"SERVERCONTROLPANEL";
const LPCWSTR ClientControlPanelName = L"CLIENTCONTROLPANEL";
const LPCWSTR FileReqDialogName = L"FILEREQDIALOGBOX";
const LPCWSTR StreamingDialogName = L"STREAMINGDIALOGBOX";

// Connection Types
const std::string ServerType = "Server";
const std::string ClientType = "Client";

// Feature Types
const std::string FileReqType = "FileRequest";
const std::string FileStreamType = "FileStream";
const std::string VoipType = "Voip";
const std::string MulticastType = "Multicast";
const std::string StreamingType = "Stream";