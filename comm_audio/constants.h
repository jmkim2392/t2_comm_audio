#pragma once
#include "main.h"

#define MAX_THREADS 20
#define DATA_BUFSIZE 8192
#define DEFAULT_REQUEST_PACKET_SIZE 64
#define FTP_PACKET_SIZE 4096
#define MAX_INPUT_LENGTH 64
#define AUDIO_BLOCK_SIZE 8192
#define AUDIO_PACKET_SIZE 8192
#define BLOCK_COUNT 100
#define MAX_NUM_STREAM_PACKETS 50

// Request types
#define WAV_FILE_REQUEST_TYPE 1
#define AUDIO_STREAM_REQUEST_TYPE 2
#define VOIP_REQUEST_TYPE 3
#define AUDIO_BUFFER_FULL_TYPE 4 // unused, can replace with any new types later
#define AUDIO_BUFFER_RDY_TYPE 5

// Control Packets
#define FILE_NOT_FOUND 17 // DC1
#define TRANSFER_COMPLETE 18 // DC2
#define TRANSFER_COMPLETE_MSG 84114971101151021011143267111109112108101116101

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

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

// String constants
const std::string disconnectedMsg = "Disconnected";
const std::string connectedMsg = "Connected";

const LPCWSTR packetMsgDelimiter = L"|";
const std::string packetMsgDelimiterStr = "|";