#pragma once
#include "main.h"

#define MAX_THREADS 20
#define DATA_BUFSIZE 8192
#define DEFAULT_REQUEST_PACKET_SIZE 64
#define FTP_PACKET_SIZE 4096

// Request types
#define WAV_FILE_REQUEST_TYPE 1
#define AUDIO_STREAM_REQUEST_TYPE 2
#define VOIP_REQUEST_TYPE 3

const LPCWSTR Name = L"Comm Audio";
const LPCWSTR MenuName = L"SERVICEMENU";