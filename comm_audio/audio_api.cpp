/*-------------------------------------------------------------------------------------
--	SOURCE FILE: audio_api.cpp - Contains audio playing functions for Comm_Audio
--
--	PROGRAM:		Comm_Audio
--
--	FUNCTIONS:
--					void initialize_audio_device();
--					void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);
--					WAVEHDR* allocateBlocks(int size, int count);
--					void freeBlocks(WAVEHDR* blockArray);
--					void writeToAudioBuffer(LPSTR data);
--					DWORD WINAPI playAudioThreadFunc(LPVOID lpParameter);
--					DWORD WINAPI bufReadySignalingThreadFunc(LPVOID lpParameter);
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Keishi Asai, Jason Kim
--
--	PROGRAMMER:		Keishi Asai, Jason Kim
--
--
--------------------------------------------------------------------------------------*/
#include "audio_api.h"

HWAVEOUT hWaveOut; /* device handle */
WAVEFORMATEX wfx;
WAVEFORMATEX wfx_win;
static CRITICAL_SECTION waveCriticalSection;
static volatile int waveFreeBlockCount;
static volatile int numFreed = MAX_NUM_STREAM_PACKETS;
static WAVEHDR* waveBlocks;
static int waveHeadBlock;
static int waveTailBlock;

HANDLE ReadyToPlayEvent;
HANDLE ReadyToSendEvent;
HANDLE AudioPlayerThread;
HANDLE BufRdySignalerThread;
HANDLE AudioRecorderThread;

BOOL isPlayingAudio = FALSE;
BOOL isRecordingAudio = FALSE;
HANDLE BufferOpenToWriteEvent;


///////////////////////////////////////////////////
// WaveIN
HWAVEIN hWaveIn;
BYTE *win_buf1;
BYTE *win_buf2;
BYTE *win_buf3;
BYTE *win_buf4;
BYTE *win_buf5;
BYTE *win_buf6;
BYTE *win_buf7;
BYTE *win_buf8;
WAVEHDR whdr1;
WAVEHDR whdr2;
WAVEHDR whdr3;
WAVEHDR whdr4;
WAVEHDR whdr5;
WAVEHDR whdr6;
WAVEHDR whdr7;
WAVEHDR whdr8;
BOOL blReset = FALSE;
//int WIN_SRATE = 44100;
int WIN_SRATE = 11025;
MMRESULT win_mret;


/*-------------------------------------------------------------------------------------
--	FUNCTION:	initialize_audio_device
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Keishi Asai, Jason Kim
--
--	PROGRAMMER:		Keishi Asai, Jason Kim
--
--	INTERFACE:		void initialize_audio_device()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to setup the audio device and audio playing feature
--------------------------------------------------------------------------------------*/
void initialize_waveout_device(WAVEFORMATEX wfxparam)
{
	// KTODO: Probably inintialize wave in device here too.
	DWORD ThreadId;

	/*
	 * initialise the module variables
	 */
	waveBlocks = allocateBlocks(AUDIO_BLOCK_SIZE, BLOCK_COUNT);
	waveFreeBlockCount = BLOCK_COUNT;
	waveHeadBlock = 0;
	waveTailBlock = 0;
	InitializeCriticalSection(&waveCriticalSection);

	initialize_events_gen(&ReadyToPlayEvent, L"AudioPlayReady");
	initialize_events_gen(&BufferOpenToWriteEvent, L"BufferReadyToWrite");
	/*
	 * set up the WAVEFORMATEX structure.
	 */
	//wfx.nSamplesPerSec = 44100; /* sample rate */
	//wfx.wBitsPerSample = 16; /* sample size */
	//wfx.nSamplesPerSec = 11025; /* sample rate */
	//wfx.wBitsPerSample = 8; /* sample size */
	//wfx.nChannels = 2; /* channels*/
	//wfx.cbSize = 0; /* size of _extra_ info */
	//wfx.wFormatTag = WAVE_FORMAT_PCM;
	//wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
	//wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

	wfx.nSamplesPerSec = wfxparam.nSamplesPerSec; /* sample rate */
	wfx.wBitsPerSample = wfxparam.wBitsPerSample; /* sample size */
	wfx.nChannels = wfxparam.nChannels; /* channels*/
	wfx.cbSize = wfxparam.cbSize; /* size of _extra_ info */
	wfx.wFormatTag = wfxparam.wFormatTag;
	wfx.nBlockAlign = wfxparam.nBlockAlign;
	wfx.nAvgBytesPerSec = wfx.nAvgBytesPerSec;

	/*
	 * try to open the default wave device. WAVE_MAPPER is
	 * a constant defined in mmsystem.h, it always points to the
	 * default wave device on the system (some people have 2 or
	 * more sound cards).
	 */
	if (waveOutOpen(
		&hWaveOut,
		WAVE_MAPPER,
		&wfx,
		(DWORD_PTR)waveOutProc,
		(DWORD_PTR)&waveFreeBlockCount,
		CALLBACK_FUNCTION
	) != MMSYSERR_NOERROR) {
		//fprintf(stderr, "%s: unable to open wave mapper device\n", argv[0]);
		ExitProcess(1);
	}

	if ((AudioPlayerThread = CreateThread(NULL, 0, playAudioThreadFunc, (LPVOID)ReadyToPlayEvent, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed creating AudioPlayerThread with error " + std::to_string(GetLastError()));
		return;
	}
	if ((BufRdySignalerThread = CreateThread(NULL, 0, bufReadySignalingThreadFunc, (LPVOID)BufferOpenToWriteEvent, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed creating BufRdySignalerThread with error " + std::to_string(GetLastError()));
		return;
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	waveOutProc
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Keishi Asai, Jason Kim
--
--	PROGRAMMER:		Keishi Asai, Jason Kim
--
--	INTERFACE:		static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
--
--	RETURNS:		void
--
--	NOTES:
--	Callback function to be called when audio device finishes playing a block
--------------------------------------------------------------------------------------*/
static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	/*
	 * pointer to free block counter
	 */
	int* freeBlockCounter = (int*)dwInstance;
	/*
	 * ignore calls that occur due to openining and closing the
	 * device.
	 */

	if (uMsg != WOM_DONE) {
		return;
	}
	numFreed++;
	//OutputDebugStringA("Hello\n");
	EnterCriticalSection(&waveCriticalSection);
	waveFreeBlockCount++;
	char debug_buf[512];
	sprintf_s(debug_buf, sizeof(debug_buf), "FreeB: %d\n", waveFreeBlockCount);
	OutputDebugStringA(debug_buf);
	//	(*freeBlockCounter)++;
	LeaveCriticalSection(&waveCriticalSection);
	TriggerEvent(ReadyToPlayEvent);
	if (numFreed >= MAX_NUM_STREAM_PACKETS && waveFreeBlockCount >= MAX_NUM_STREAM_PACKETS) {
		numFreed = 1;
		TriggerEvent(BufferOpenToWriteEvent);
	}
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	writeToAudioBuffer
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Keishi Asai, Jason Kim
--
--	PROGRAMMER:		Keishi Asai, Jason Kim
--
--	INTERFACE:		void writeToAudioBuffer(LPSTR data)
--									LPSTR data - the audio data to write to block
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to write data into the audio circular buffer and begin playing audio
--------------------------------------------------------------------------------------*/
void writeToAudioBuffer(LPSTR data)
{
	WAVEHDR* head;
	head = &waveBlocks[waveHeadBlock];

	if (head->dwFlags & WHDR_PREPARED)
	{
		waveOutUnprepareHeader(hWaveOut, head, sizeof(WAVEHDR));
	}

	memcpy(head->lpData + head->dwUser, data, AUDIO_BLOCK_SIZE);
	head->dwBufferLength = AUDIO_BLOCK_SIZE;

	EnterCriticalSection(&waveCriticalSection);
	waveFreeBlockCount--;
	LeaveCriticalSection(&waveCriticalSection);
	waveHeadBlock++;
	waveHeadBlock %= BLOCK_COUNT;
	head->dwUser = 0;
	TriggerEvent(ReadyToPlayEvent);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	playAudioThreadFunc
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Keishi Asai, Jason Kim
--
--	PROGRAMMER:		Keishi Asai, Jason Kim
--
--	INTERFACE:		DWORD WINAPI writeToAudioBuffer(LPVOID lpParameter)
--									LPSTR data - the audio data to write to block
--
--	RETURNS:		DWORD
--
--	NOTES:
--	Thread function to read from audio circular buffer and play the data block
--------------------------------------------------------------------------------------*/
DWORD WINAPI playAudioThreadFunc(LPVOID lpParameter) 
{
	WAVEHDR* tail;
	DWORD Index;
	isPlayingAudio = TRUE;
	HANDLE readyEvent = (HANDLE)lpParameter;

	while (isPlayingAudio)
	{
		WaitForSingleObject(readyEvent, INFINITE);
		ResetEvent(readyEvent);
		while (waveTailBlock != waveHeadBlock) 
		{
			tail = &waveBlocks[waveTailBlock];
			waveOutPrepareHeader(hWaveOut, tail, sizeof(WAVEHDR));
			waveOutWrite(hWaveOut, tail, sizeof(WAVEHDR));
			waveTailBlock++;
			waveTailBlock %= BLOCK_COUNT;
		}
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	bufReadySignalingThreadFunc
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		DWORD WINAPI bufReadySignalingThreadFunc(LPVOID lpParameter)
--									LPVOID lpParameter - event to listen
--
--	RETURNS:		DWORD
--
--	NOTES:
--	Thread function to signal the server to send next set of blocks to stream
--------------------------------------------------------------------------------------*/
DWORD WINAPI bufReadySignalingThreadFunc(LPVOID lpParameter)
{
	isPlayingAudio = TRUE;
	HANDLE readyEvent = (HANDLE)lpParameter;

	while (isPlayingAudio)
	{
		WaitForSingleObject(readyEvent, INFINITE);
		ResetEvent(readyEvent);
		send_request(AUDIO_BUFFER_RDY_TYPE, L"RDY");
	}
	return 0;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	allocateBlocks
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Keishi Asai
--
--	PROGRAMMER:		Keishi Asai
--
--	INTERFACE:		WAVEHDR* allocateBlocks(int size, int count)
--									int size - the size of the data buffer
--									int count - the size of the circular buffer
--
--	RETURNS:		WAVEHDR*
--
--	NOTES:
--	Call this function to allocate a memory for the audio circular buffer
--------------------------------------------------------------------------------------*/
WAVEHDR* allocateBlocks(int size, int count)
{
	unsigned char* buffer;
	int i;
	WAVEHDR* blocks;
	DWORD totalBufferSize = (size + sizeof(WAVEHDR)) * count;
	/*
	 * allocate memory for the entire set in one go
	 */
	if ((buffer = (unsigned char*)HeapAlloc(
		GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		totalBufferSize
	)) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		ExitProcess(1);
	}
	/*
	 * and set up the pointers to each bit
	 */
	blocks = (WAVEHDR*)buffer;
	buffer += sizeof(WAVEHDR) * count;
	for (i = 0; i < count; i++) {
		blocks[i].dwBufferLength = size;
		// May break by casting
		blocks[i].lpData = (LPSTR)buffer;
		buffer += size;
	}
	return blocks;
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	freeBlocks
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Keishi Asai
--
--	PROGRAMMER:		Keishi Asai
--
--	INTERFACE:		void freeBlocks(WAVEHDR* blockArray)
--									WAVEHDR* blockArray - the circular buffer to free
--
--	RETURNS:		WAVEHDR*
--
--	NOTES:
--	Call this function to deallocate audio circular buffer's memory
--------------------------------------------------------------------------------------*/
void freeBlocks(WAVEHDR* blockArray)
{
	/*
	 * and this is why allocateBlocks works the way it does
	 */
	HeapFree(GetProcessHeap(), 0, blockArray);
}

/*-------------------------------------------------------------------------------------
--	FUNCTION:	terminate_audio_api
--
--	DATE:			March 31, 2019
--
--	REVISIONS:		March 31, 2019
--
--	DESIGNER:		Jason Kim
--
--	PROGRAMMER:		Jason Kim
--
--	INTERFACE:		void terminate_audio_api()
--
--	RETURNS:		void
--
--	NOTES:
--	Call this function to terminate the audio api
--	TODO: may need to implement later, for now nothing
--------------------------------------------------------------------------------------*/
void terminate_audio_api() {

}

/////////////////////////////////////////////////////////////////////////////////////////////
// Wave In

void initialize_wavein_device(HWND hWndDlg)
{
	// KTODO: if this works and have time, change it to array or sth
	win_buf1 = (BYTE*)malloc(WIN_SRATE);
	win_buf2 = (BYTE*)malloc(WIN_SRATE);
	win_buf3 = (BYTE*)malloc(WIN_SRATE);
	win_buf4 = (BYTE*)malloc(WIN_SRATE);
	win_buf5 = (BYTE*)malloc(WIN_SRATE);
	win_buf6 = (BYTE*)malloc(WIN_SRATE);
	win_buf7 = (BYTE*)malloc(WIN_SRATE);
	win_buf8 = (BYTE*)malloc(WIN_SRATE);

	wfx_win.nSamplesPerSec = WIN_SRATE; /* sample rate */
	//wfx_win.wBitsPerSample = 16; /* sample size */
	wfx_win.wBitsPerSample = 8; /* sample size */
	wfx_win.nChannels = 2; /* channels*/
	wfx_win.cbSize = 0; /* size of _extra_ info */
	wfx_win.wFormatTag = WAVE_FORMAT_PCM;
	wfx_win.nBlockAlign = (wfx_win.wBitsPerSample * wfx_win.nChannels) >> 3;
	wfx_win.nAvgBytesPerSec = wfx_win.nBlockAlign * wfx_win.nSamplesPerSec;

	whdr1.lpData = (LPSTR)win_buf1;
	whdr1.dwBufferLength = WIN_SRATE;
	whdr1.dwBytesRecorded = 0;
	whdr1.dwFlags = 0;
	whdr1.dwLoops = 1;
	whdr1.lpNext = NULL;
	whdr1.dwUser = 0;
	whdr1.reserved = 0;

	whdr2.lpData = (LPSTR)win_buf2;
	whdr2.dwBufferLength = WIN_SRATE;
	whdr2.dwBytesRecorded = 0;
	whdr2.dwFlags = 0;
	whdr2.dwLoops = 1;
	whdr2.lpNext = NULL;
	whdr2.dwUser = 0;
	whdr2.reserved = 0;

	whdr3.lpData = (LPSTR)win_buf3;
	whdr3.dwBufferLength = WIN_SRATE;
	whdr3.dwBytesRecorded = 0;
	whdr3.dwFlags = 0;
	whdr3.dwLoops = 1;
	whdr3.lpNext = NULL;
	whdr3.dwUser = 0;
	whdr3.reserved = 0;

	whdr4.lpData = (LPSTR)win_buf4;
	whdr4.dwBufferLength = WIN_SRATE;
	whdr4.dwBytesRecorded = 0;
	whdr4.dwFlags = 0;
	whdr4.dwLoops = 1;
	whdr4.lpNext = NULL;
	whdr4.dwUser = 0;
	whdr4.reserved = 0;

	whdr5.lpData = (LPSTR)win_buf5;
	whdr5.dwBufferLength = WIN_SRATE;
	whdr5.dwBytesRecorded = 0;
	whdr5.dwFlags = 0;
	whdr5.dwLoops = 1;
	whdr5.lpNext = NULL;
	whdr5.dwUser = 0;
	whdr5.reserved = 0;

	whdr6.lpData = (LPSTR)win_buf6;
	whdr6.dwBufferLength = WIN_SRATE;
	whdr6.dwBytesRecorded = 0;
	whdr6.dwFlags = 0;
	whdr6.dwLoops = 1;
	whdr6.lpNext = NULL;
	whdr6.dwUser = 0;
	whdr6.reserved = 0;

	whdr7.lpData = (LPSTR)win_buf7;
	whdr7.dwBufferLength = WIN_SRATE;
	whdr7.dwBytesRecorded = 0;
	whdr7.dwFlags = 0;
	whdr7.dwLoops = 1;
	whdr7.lpNext = NULL;
	whdr7.dwUser = 0;
	whdr7.reserved = 0;

	whdr8.lpData = (LPSTR)win_buf8;
	whdr8.dwBufferLength = WIN_SRATE;
	whdr8.dwBytesRecorded = 0;
	whdr8.dwFlags = 0;
	whdr8.dwLoops = 1;
	whdr8.lpNext = NULL;
	whdr8.dwUser = 0;
	whdr8.reserved = 0;


	//win_mret = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx_win, (DWORD)parent_hwnd, 0, CALLBACK_WINDOW);
	win_mret = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx_win, (DWORD)hWndDlg, 0, CALLBACK_WINDOW);
	//win_mret = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx_win, (DWORD_PTR)waveInProc, 0, CALLBACK_FUNCTION);
	if (win_mret == MMSYSERR_NOERROR) {
		OutputDebugStringA("good");
		// From WIM_OPEN

		win_mret = waveInPrepareHeader(hWaveIn, &whdr1, sizeof(WAVEHDR));
		if (win_mret == MMSYSERR_NOERROR) {
			OutputDebugStringA("good");
		}
		else {
			OutputDebugStringA("bad");
		}
		win_mret = waveInPrepareHeader(hWaveIn, &whdr2, sizeof(WAVEHDR));
		if (win_mret == MMSYSERR_NOERROR) {
			OutputDebugStringA("good");
		}
		else {
			OutputDebugStringA("bad");
		}
	}

}

//void startRecording(HANDLE ReadyToSendEvent)
void startRecording()
{
	//DWORD ThreadId;

	// initialize waveIn module variables
	//waveInBlocks = allocateBlocks(AUDIO_BLOCK_SIZE, BLOCK_COUNT);
	//waveInFreeBlockCount = BLOCK_COUNT;
	//waveInHeadBlock = 0;
	//waveInTailBlock = 0;
	//InitializeCriticalSection(&waveInCriticalSection);

	// try to open the default wave in device
	//if (waveInOpen(
	//	&hWaveIn,
	//	WAVE_MAPPER,
	//	&wfx,
	//	(DWORD_PTR)waveInProc,
	//	(DWORD_PTR)&waveInFreeBlockCount,
	//	CALLBACK_FUNCTION
	//) != MMSYSERR_NOERROR) {
	//	ExitProcess(1);
	//}
	waveInStart(hWaveIn);

	// start recordAudioThreadFunc thread
	//if ((AudioRecorderThread = CreateThread(NULL, 0, recordAudioThreadFunc, (LPVOID)ReadyToSendEvent, 0, &ThreadId)) == NULL)
	//{
	//	update_client_msgs("Failed creating AudioRecorderThread with error " + std::to_string(GetLastError()));
	//	return;
	//}
}

void wave_in_add_buffer(PWAVEHDR pwhdr, size_t size) 
{
	waveInAddBuffer(hWaveIn, pwhdr, size);
}

void wave_in_add_buffer()
{
	win_mret = waveInAddBuffer(hWaveIn, &whdr1, sizeof(WAVEHDR));
	if (win_mret == MMSYSERR_NOERROR) {
		OutputDebugStringA("good");
	}
	else {
		OutputDebugStringA("bad add");
	}
	win_mret = waveInAddBuffer(hWaveIn, &whdr2, sizeof(WAVEHDR));
	if (win_mret == MMSYSERR_NOERROR) {
		OutputDebugStringA("ccc");
	}
	else {
		OutputDebugStringA("bad add2");
	}
	win_mret = waveInAddBuffer(hWaveIn, &whdr3, sizeof(WAVEHDR));
	win_mret = waveInAddBuffer(hWaveIn, &whdr4, sizeof(WAVEHDR));
	win_mret = waveInAddBuffer(hWaveIn, &whdr5, sizeof(WAVEHDR));
	win_mret = waveInAddBuffer(hWaveIn, &whdr6, sizeof(WAVEHDR));
	win_mret = waveInAddBuffer(hWaveIn, &whdr7, sizeof(WAVEHDR));
	win_mret = waveInAddBuffer(hWaveIn, &whdr8, sizeof(WAVEHDR));
}



void close_win_device()
{
	waveInUnprepareHeader(hWaveIn, &whdr1, sizeof(WAVEHDR));
	waveInUnprepareHeader(hWaveIn, &whdr2, sizeof(WAVEHDR));
	waveInUnprepareHeader(hWaveIn, &whdr3, sizeof(WAVEHDR));
	waveInUnprepareHeader(hWaveIn, &whdr4, sizeof(WAVEHDR));
	waveInUnprepareHeader(hWaveIn, &whdr5, sizeof(WAVEHDR));
	waveInUnprepareHeader(hWaveIn, &whdr6, sizeof(WAVEHDR));
	waveInUnprepareHeader(hWaveIn, &whdr7, sizeof(WAVEHDR));
	waveInUnprepareHeader(hWaveIn, &whdr8, sizeof(WAVEHDR));
	free(win_buf1);
	free(win_buf2);
	free(win_buf3);
	free(win_buf4);
	free(win_buf5);
	free(win_buf6);
	free(win_buf7);
	free(win_buf8);
}

//static void CALLBACK waveInProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
//{
	/*
	 * pointer to free block counter
	 */
	//int* freeBlockCounter = (int*)dwInstance;
	/*
	 * ignore calls that occur due to openining and closing the
	 * device.
	 */

	//if (uMsg != WOM_DONE) {
	//	return;
	//}

	// DASHA - not sure what to do here....
	//waveInNumFreed++;

	//EnterCriticalSection(&waveInCriticalSection);
	//waveInFreeBlockCount++;
	//char debug_buf[512];
	//sprintf_s(debug_buf, sizeof(debug_buf), "FreeB: %d\n", waveInFreeBlockCount);
	//OutputDebugStringA(debug_buf);
	//LeaveCriticalSection(&waveInCriticalSection);

	////TriggerEvent(ReadyToPlayEvent);
	//if (waveInNumFreed >= MAX_NUM_STREAM_PACKETS && waveInFreeBlockCount >= MAX_NUM_STREAM_PACKETS) {
	//	waveInNumFreed = 1;
	//}
//}

//DWORD WINAPI recordAudioThreadFunc(LPVOID lpParameter)
//{
//	WAVEHDR* head;
//	WAVEHDR* tail;
//	DWORD Index;
//	isRecordingAudio = TRUE;
//	HANDLE readyEvent = (HANDLE)lpParameter;
//
//	// DASHA - not sure what to do here...
//	// when do you use head and when do you use tail?
//
//	while (isRecordingAudio)
//	{
//		while (waveInTailBlock != waveInHeadBlock)
//		{
//			//tail = &waveInBlocks[waveInTailBlock];
//			head = &waveInBlocks[waveInHeadBlock];
//
//			waveInPrepareHeader(hWaveIn, head, sizeof(WAVEHDR));
//			waveInAddBuffer(hWaveIn, head, sizeof(WAVEHDR));
//			waveInStart(hWaveIn);
//
//			//waveInTailBlock++;
//			//waveInTailBlock %= BLOCK_COUNT;
//			waveInHeadBlock++;
//			waveInHeadBlock %= BLOCK_COUNT;
//		}
//
//		TriggerEvent(readyEvent); //ReadyToSend
//	}
//	return 0;
//}

//LPSTR getRecordedAudioBuffer()
//{
	// DASHA - not sure what to do here....
	// return waveInBlocks? first block?
	//return "helloc";
//}
//static void CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD lp)
//{
//
//	switch (uMsg) {
//
//	case WIM_OPEN:
//		OutputDebugStringA("open");
//		break;
//	case WIM_DATA:
//		// post message to process this input block received
//		// NOTE: callback cannot call other waveform functions
//		//....................................................
//
//		//PostMessage((HWND)dwInstance, USR_INBLOCK, 0, dwParam1);
//
//
//		// Dont need for network
//		OutputDebugStringA("data");
//		char sbuf[512];
//		sprintf_s(sbuf, "%d\n", ((PWAVEHDR)lp)->dwBytesRecorded);
//		update_client_msgs(sbuf);
//		// Dont need for network
//
//		//if (blReset || !bTmp) {
//		//if (blReset) {
//			//			waveInClose(hWaveIn);
//			//blReset = FALSE;
//			//return 0;
//			//break;
//		//}
//
//
//		// Call WSASend Recv here
//		// WSASend(socket_to_send_to_peer, ((PWAVEHDR)lp).lpData, 1, ((PWAVEHDR)lp).dwBytesRecorded, 0, &Overlapped, NULL)
//
//		//waveInAddBuffer(hWaveIn, (PWAVEHDR)lp, sizeof(WAVEHDR));
//
//		break;
//	case WIM_CLOSE:
//		OutputDebugStringA("wim close");
//		//waveInUnprepareHeader(hWaveIn, &whdr1, sizeof(WAVEHDR));
//		//waveInUnprepareHeader(hWaveIn, &whdr2, sizeof(WAVEHDR));
//		free(win_buf1);
//		free(win_buf2);
//		break; // don't care
//
//	}
//}
//
