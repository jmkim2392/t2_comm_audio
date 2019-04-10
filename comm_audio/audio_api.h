#pragma once
#pragma comment( lib, "Winmm.lib" )
#include "main.h"
#include <mmsystem.h>
#include "client.h"

void initialize_waveout_device(WAVEFORMATEX wfxparam, BOOL multicastFlag, int blockSize);
void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);
WAVEHDR* allocateBlocks(int size, int count);
void freeBlocks(WAVEHDR* blockArray);
void writeToAudioBuffer(LPSTR data, int blockSize);
DWORD WINAPI playAudioThreadFunc(LPVOID lpParameter);
DWORD WINAPI bufReadySignalingThreadFunc(LPVOID lpParameter);

void initialize_wavein_device(HWND);
void startRecording();
static void CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD lp);

DWORD WINAPI recordAudioThreadFunc(LPVOID lpParameter);
LPSTR getRecordedAudioBuffer();
void wave_in_add_buffer(PWAVEHDR pwhdr, size_t size);
void wave_in_add_buffer();
void close_win_device();
void terminateAudioApi();
