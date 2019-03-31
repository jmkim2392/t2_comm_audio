#include "audio_api.h"

HWAVEOUT hWaveOut; /* device handle */
WAVEFORMATEX wfx; /* look this up in your documentation */
static CRITICAL_SECTION waveCriticalSection;
static volatile int waveFreeBlockCount;
static WAVEHDR* waveBlocks;
static int waveCurrentBlock;

void initialize_audio_device() {
	/*
	 * initialise the module variables
	 */
	waveBlocks = allocateBlocks(AUDIO_BLOCK_SIZE, BLOCK_COUNT);
	waveFreeBlockCount = BLOCK_COUNT;
	waveCurrentBlock = 0;
	InitializeCriticalSection(&waveCriticalSection);

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
}

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
	if (uMsg != WOM_DONE)
		return;
	EnterCriticalSection(&waveCriticalSection);
	(*freeBlockCounter)++;
	LeaveCriticalSection(&waveCriticalSection);
}

void writeAudio(LPSTR data, int size)
{
	WAVEHDR* current;
	int remain;
	current = &waveBlocks[waveCurrentBlock];
	while (size > 0) {
		/*
		 * first make sure the header we're going to use is unprepared
		 */
		if (current->dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		if (size < (int)(AUDIO_BLOCK_SIZE - current->dwUser)) {
			memcpy(current->lpData + current->dwUser, data, size);
			current->dwUser += size;
			break;
		}
		remain = AUDIO_BLOCK_SIZE - current->dwUser;
		memcpy(current->lpData + current->dwUser, data, remain);
		size -= remain;
		data += remain;
		current->dwBufferLength = AUDIO_BLOCK_SIZE;
		
		waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
		
		EnterCriticalSection(&waveCriticalSection);
		waveFreeBlockCount--;
		LeaveCriticalSection(&waveCriticalSection);
		/*
		 * wait for a block to become free
		 */
		while (!waveFreeBlockCount)
			Sleep(10);
		/*
		 * point to the next block
		 */
		waveCurrentBlock++;
		waveCurrentBlock %= BLOCK_COUNT;
		current = &waveBlocks[waveCurrentBlock];
		current->dwUser = 0;
	}
}

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

void freeBlocks(WAVEHDR* blockArray)
{
	/*
	 * and this is why allocateBlocks works the way it does
	 */
	HeapFree(GetProcessHeap(), 0, blockArray);
}

void terminate_audio_api() {

}