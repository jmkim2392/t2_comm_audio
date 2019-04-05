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
WAVEFORMATEX wfx; /* look this up in your documentation */
static CRITICAL_SECTION waveCriticalSection;
static volatile int waveFreeBlockCount;
static volatile int numFreed = MAX_NUM_STREAM_PACKETS;
static WAVEHDR* waveBlocks;
static int waveHeadBlock;
static int waveTailBlock;

HWAVEIN hWaveIn;
// can i use wfx for wave in as well?
static CRITICAL_SECTION waveInCriticalSection;
static volatile int waveInFreeBlockCount;
static volatile int waveInNumFreed = MAX_NUM_STREAM_PACKETS;
static WAVEHDR* waveInBlocks;
static int waveInHeadBlock;
static int waveInTailBlock;

HANDLE ReadyToPlayEvent;
HANDLE ReadyToSendEvent;
HANDLE AudioPlayerThread;
HANDLE BufRdySignalerThread;
HANDLE AudioRecorderThread;

BOOL isPlayingAudio = FALSE;
BOOL isRecordingAudio = FALSE;
HANDLE BufferOpenToWriteEvent;

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
void initialize_audio_device()
{
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
	wfx.nSamplesPerSec = 44100; /* sample rate */
	wfx.wBitsPerSample = 16; /* sample size */
	wfx.nChannels = 2; /* channels*/
	wfx.cbSize = 0; /* size of _extra_ info */
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
	wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
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

void startWaveIn()
{
	DWORD ThreadId;

	// initialize waveIn module variables
	waveInBlocks = allocateBlocks(AUDIO_BLOCK_SIZE, BLOCK_COUNT);
	waveInFreeBlockCount = BLOCK_COUNT;
	waveInHeadBlock = 0;
	waveInTailBlock = 0;
	InitializeCriticalSection(&waveInCriticalSection);

	initialize_events_gen(&ReadyToSendEvent, L"AudioSendReady");

	// try to open the default wave in device
	if (waveInOpen(
		&hWaveIn,
		WAVE_MAPPER,
		&wfx,
		(DWORD_PTR)waveInProc,
		(DWORD_PTR)&waveInFreeBlockCount,
		CALLBACK_FUNCTION
	) != MMSYSERR_NOERROR) {
		ExitProcess(1);
	}

	// start recordAudioThreadFunc thread
	if ((AudioRecorderThread = CreateThread(NULL, 0, recordAudioThreadFunc, (LPVOID)ReadyToSendEvent, 0, &ThreadId)) == NULL)
	{
		update_client_msgs("Failed creating AudioRecorderThread with error " + std::to_string(GetLastError()));
		return;
	}
}

static void CALLBACK waveInProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
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

	// DASHA - not sure what to do here....
	//waveInNumFreed++;

	//EnterCriticalSection(&waveInCriticalSection);
	//waveInFreeBlockCount++;
	//char debug_buf[512];
	//sprintf_s(debug_buf, sizeof(debug_buf), "FreeB: %d\n", waveInFreeBlockCount);
	//OutputDebugStringA(debug_buf);
	//LeaveCriticalSection(&waveCriticalSection);

	//TriggerEvent(ReadyToPlayEvent);
	//if (waveInNumFreed >= MAX_NUM_STREAM_PACKETS && waveInFreeBlockCount >= MAX_NUM_STREAM_PACKETS) {
	//	waveInNumFreed = 1;
	//}
}

DWORD WINAPI recordAudioThreadFunc(LPVOID lpParameter)
{
	WAVEHDR* tail;
	DWORD Index;
	isRecordingAudio = TRUE;
	HANDLE readyEvent = (HANDLE)lpParameter;

	// DASHA - not sure what to do here....
	//while (isRecordingAudio)
	//{
	//	/*WaitForSingleObject(readyEvent, INFINITE);
	//	ResetEvent(readyEvent);*/
	//	while (waveInTailBlock != waveInHeadBlock)
	//	{
	//		tail = &waveInBlocks[waveInTailBlock];
	//		waveInPrepareHeader(hWaveIn, tail, sizeof(WAVEHDR));

	//		waveInAddBuffer(hWaveIn, tail, sizeof(WAVEHDR));
	//		waveInStart(hWaveIn);

	//		waveInTailBlock++;
	//		waveInTailBlock %= BLOCK_COUNT;
	//	}

	//	TriggerEvent(readyEvent);
	//}
	return 0;
}

LPSTR getRecordedAudioBuffer()
{
	// DASHA - not sure what to do here....
	// return waveInBlocks? first block?
}