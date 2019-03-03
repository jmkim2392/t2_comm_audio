#pragma once
#include "main.h"

void CALLBACK RequestProcessRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags)