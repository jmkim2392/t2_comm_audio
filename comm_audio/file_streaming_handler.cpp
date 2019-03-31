#include "file_streaming_handler.h"

using namespace std;

FILE* requested_file_stream;
DWORD SendBytes_FileStream;

LPSOCKET_INFORMATION FileStreamSocketInfo;
BOOL isReceivingFileStream = FALSE;

SOCKADDR_IN client;
int client_len = sizeof(sockaddr_in);

char fileStream_complete[1];
char fileStream_not_found[1];
char *file_stream_buf;

void initialize_file_stream(SOCKET* socket, SOCKADDR_IN* addr, WSAEVENT fileStreamCompletedEvent)
{

	file_stream_buf = (char*)malloc(AUDIO_BLOCK_SIZE);

	// Create a socket information structure
	if ((FileStreamSocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		//terminate_connection();
		return;
	}

	// Fill in the details of the server socket to receive file from
	FileStreamSocketInfo->Socket = *socket;
	ZeroMemory(&(FileStreamSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	FileStreamSocketInfo->BytesSEND = 0;
	FileStreamSocketInfo->BytesRECV = 0;
	if (addr != NULL) {
		FileStreamSocketInfo->Sock_addr = *addr;
	}
	FileStreamSocketInfo->DataBuf.len = AUDIO_BLOCK_SIZE;
	FileStreamSocketInfo->DataBuf.buf = FileStreamSocketInfo->AUDIO_BUFFER;
	if (fileStreamCompletedEvent != NULL)
	{
		FileStreamSocketInfo->CompletedEvent = fileStreamCompletedEvent;
	}
}
int open_file_to_stream(std::string filename)
{
	return fopen_s(&requested_file_stream, filename.c_str(), "rb");
}
void close_file_streaming() {

}
void start_sending_file_stream()
{
	memset(file_stream_buf, 0, AUDIO_BLOCK_SIZE);
	int bytes_read = fread(file_stream_buf, 1, AUDIO_BLOCK_SIZE, requested_file_stream);

	memcpy(FileStreamSocketInfo->DataBuf.buf, file_stream_buf, bytes_read);
	FileStreamSocketInfo->DataBuf.len = bytes_read;

	if (WSASendTo(FileStreamSocketInfo->Socket, &(FileStreamSocketInfo->DataBuf), 1, &SendBytes_FileStream, 0, (SOCKADDR *)&(FileStreamSocketInfo->Sock_addr), sizeof(FileStreamSocketInfo->Sock_addr), &(FileStreamSocketInfo->Overlapped), FileStream_SendRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}
void send_file_not_found_packet_udp() {

}

DWORD WINAPI ReceiveStreamThreadFunc(LPVOID lpParameter)
{
	DWORD Index;
	WSAEVENT EventArray[1];
	isReceivingFileStream = TRUE;
	EventArray[0] = (WSAEVENT) lpParameter;

	start_receiving_stream();

	while (isReceivingFileStream)
	{
		while (isReceivingFileStream)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				update_client_msgs("WSAWaitForMultipleEvents failed with error " + std::to_string(WSAGetLastError()));
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				break;
			}

			if (!isReceivingFileStream) {
				close_file_streaming();
			}
		}
	}

	return 0;
}
void start_receiving_stream()
{
	size_t i;
	DWORD RecvBytes;

	DWORD Flags = 0;

	ZeroMemory(&(FileStreamSocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	FileStreamSocketInfo->DataBuf.buf = FileStreamSocketInfo->AUDIO_BUFFER;
	FileStreamSocketInfo->DataBuf.len = AUDIO_BLOCK_SIZE;

	Flags = 0;

	if (WSARecvFrom(FileStreamSocketInfo->Socket, &(FileStreamSocketInfo->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& client, &client_len, &(FileStreamSocketInfo->Overlapped), FileStream_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecvFrom() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}

void CALLBACK FileStream_ReceiveRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;

	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;
	if (Error != 0)
	{
		update_client_msgs("I/O operation failed with error " + std::to_string(Error));
	}

	if (BytesTransferred == 0 || BytesTransferred == 1)
	{
		//if (strncmp(SI->DataBuf.buf, file_not_found_packet, 1) == 0)
		//{
		//	//FILE NOT FOUND PROCESS
		//	//update_client_msgs("File Not Found on server.");
		//	finalize_ftp("File Not Found on server.");
		//}
		//else if (strncmp(SI->DataBuf.buf, ftp_complete_packet, 1) == 0)
		//{
		//	//FTP COMPLETE PROCESS
		//	//update_client_msgs("File transfer completed.");
		//	finalize_ftp("File transfer completed.");
		//}
		update_client_msgs("Closing ftp socket " + std::to_string(SI->Socket));

		isReceivingFileStream = FALSE;
		TriggerEvent(SI->CompletedEvent);
		return;
	}

	/*if (BytesTransferred != AUDIO_BLOCK) {
		if (SI->DataBuf.buf[BytesTransferred] == FILE_NOT_FOUND) {
			finalize_ftp("File Not Found on server.");
		}
		else if (SI->DataBuf.buf[BytesTransferred] == FTP_COMPLETE) {
			finalize_ftp("File transfer completed.");
		}
	}*/

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	//write_file(SI->DataBuf.buf, BytesTransferred);
	//writeAudio(SI->DataBuf.buf, BytesTransferred);

	SI->DataBuf.buf = SI->AUDIO_BUFFER;
	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, (SOCKADDR *)& client, &client_len, &(SI->Overlapped), FileStream_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			update_client_msgs("WSARecv() failed with error " + std::to_string(WSAGetLastError()));
			return;
		}
	}
}
void CALLBACK FileStream_SendRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	size_t bytes_read;
	// Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0)
	{
		update_server_msgs("I/O operation failed with error " + std::to_string(Error));
	}

	if (BytesTransferred == 0)
	{
		update_server_msgs("Closing ftp socket " + std::to_string(SI->Socket));
	}

	if (Error != 0 || BytesTransferred == 1)
	{
		GlobalFree(SI);
		return;
	}

	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	if (!feof(requested_file_stream))
	{
		memset(file_stream_buf, 0, strlen(file_stream_buf));
		bytes_read = fread(file_stream_buf, 1, AUDIO_BLOCK_SIZE, requested_file_stream);

		memcpy(SI->DataBuf.buf, file_stream_buf, bytes_read);
		SI->DataBuf.len = (ULONG)bytes_read;

		if (WSASendTo(SI->Socket, &(SI->DataBuf), 1, &SendBytes_FileStream, 0, (SOCKADDR *)&(SI->Sock_addr), sizeof(SI->Sock_addr), &(SI->Overlapped), FileStream_SendRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
				return;
			}
		}
	}
	else
	{
		//SI->DataBuf.buf = ftp_complete_packet;
		SI->DataBuf.len = 1;
		update_server_msgs("File Stream completed");
		/*if (WSASend(SI->Socket, &(SI->DataBuf), 1, &SendBytes, 0,
			&(SI->Overlapped), NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				update_server_msgs("WSASend() failed with error " + std::to_string(WSAGetLastError()));
				return;
			}
		}*/
	}
}

