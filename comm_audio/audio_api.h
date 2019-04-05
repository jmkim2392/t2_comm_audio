#pragma once
#pragma comment( lib, "Winmm.lib" )
#include "main.h"
#include <mmsystem.h>
#include "client.h"

void initialize_audio_device();
void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);
WAVEHDR* allocateBlocks(int size, int count);
void freeBlocks(WAVEHDR* blockArray);
void writeToAudioBuffer(LPSTR data);
DWORD WINAPI playAudioThreadFunc(LPVOID lpParameter);
DWORD WINAPI bufReadySignalingThreadFunc(LPVOID lpParameter);

void startWaveIn();
void CALLBACK waveInProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);
DWORD WINAPI recordAudioThreadFunc(LPVOID lpParameter);
LPSTR getRecordedAudioBuffer();