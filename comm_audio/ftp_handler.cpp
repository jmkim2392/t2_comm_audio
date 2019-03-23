#include "ftp_handler.h"

using namespace std;

ifstream fin;
ofstream fout;
FILE* requested_file;
FILE* file_output;
DWORD SendBytes;
LPSOCKET_INFORMATION SocketInfo;
BOOL isReceivingFile = FALSE;

char end_ftp_buf[1];
char buffer[FTP_PACKET_SIZE];
char *buf;
circular_buffer<std::string> ftp_buffer(30);

void initialize_ftp(SOCKET* socket, WSAEVENT ftp_packet_recved) 
{
	buf = (char*)malloc(FTP_PACKET_SIZE);
	// Create a socket information structure to associate with the accepted socket.
	if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		printf("GlobalAlloc() failed with error %d\n", GetLastError());
		terminate_connection();
		return;
	}

	// Fill in the details of the server socket to receive file from
	SocketInfo->Socket = *socket;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	SocketInfo->BytesSEND = 0;
	SocketInfo->BytesRECV = 0;
	SocketInfo->DataBuf.len = FTP_PACKET_SIZE;
	SocketInfo->DataBuf.buf = buffer;
	if (ftp_packet_recved != NULL) 
	{
		SocketInfo->CompletedEvent = ftp_packet_recved;
	}
	
}

// deprecate and use fopen_s instead
void read_file(std::string filename) {
	fin.open(filename, ios::in );
	open_file(filename);
}

void open_file(std::string filename) {
	fopen_s(&requested_file, filename.c_str(), "rb");
}

void create_new_file(std::string filename) {
	fout.open(filename, ios::binary | ios::out );
}

void close_file() {

}
// deprecated and use fwrite instead 
void write_file(std::string data) {
	fout.write(data.c_str(), data.length());
}

void start_sending_file() {

	//TODO: check if file exists first 
	memset(buf, 0, strlen(buf));
	fread(buf, 1, FTP_PACKET_SIZE, requested_file);
	SocketInfo->DataBuf.buf = buf;

	if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, 0,
		&(SocketInfo->Overlapped), FTP_SendRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			int temp = WSAGetLastError();
			printf("WSASend() failed with error %d\n", WSAGetLastError());
			return;
		}
	}
}

void start_receiving_file(int type, LPCWSTR request) {
	size_t i;
	DWORD SendBytes;
	char* req_msg = (char *)malloc(MAX_INPUT_LENGTH);
	std::string packet;

	wcstombs_s(&i, req_msg, MAX_INPUT_LENGTH, request, MAX_INPUT_LENGTH);

	std::string file_req_packet = generateRequestPacket(type, req_msg);

	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	SocketInfo->DataBuf.buf = (char*)file_req_packet.c_str();
	SocketInfo->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;

	if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, 0, &(SocketInfo->Overlapped), FTP_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			return;
		}
	}
}

DWORD WINAPI ReceiveFileThreadFunc(LPVOID lpParameter)
{
	DWORD Index;
	WSAEVENT EventArray[1];
	isReceivingFile = TRUE;
	LPFTP_INFO info = (LPFTP_INFO)lpParameter;
	EventArray[0] = info->packetRecvEvent;

	start_receiving_file(WAV_FILE_REQUEST_TYPE, info->filename);

	while (isReceivingFile)
	{
		while (isReceivingFile)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				printf("WSAWaitForMultipleEvents failed with error %d\n", WSAGetLastError());
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				break;
			}
		}
	}

	return 0;
}

DWORD WINAPI ReceiveFile(LPVOID lpParameter)
{
	std::string dataPacket;
	DWORD Index;
	WSAEVENT EventArray[1];
	int count = 0;

	isReceivingFile = TRUE;
	EventArray[0] = (WSAEVENT)lpParameter;

	while (isReceivingFile)
	{
		while (isReceivingFile)
		{
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);

			if (Index == WSA_WAIT_FAILED)
			{
				printf("WSAWaitForMultipleEvents failed with error %d\n", WSAGetLastError());
				terminate_connection();
				return FALSE;
			}

			if (Index != WAIT_IO_COMPLETION)
			{
				// An accept() call event is ready - break the wait loop
				break;
			}
		}

		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);
		while (!ftp_buffer.empty()) {
			dataPacket = ftp_buffer.get();
			count++;
			if (!dataPacket.empty()) {
				write_file(dataPacket);
			}
		}
	}

	return 0;
}

void CALLBACK FTP_ReceiveRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;
	// need to change to char* ? might not be an issue
	std::string packet;

	// Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0)
	{
		printf("I/O operation failed with error %d\n", Error);
	}

	if (BytesTransferred == 0)
	{
		printf("Closing socket %d\n", SI->Socket);
		isReceivingFile = FALSE;
		TriggerEvent(SI->CompletedEvent);
		return;
	}

	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	if (BytesTransferred == FTP_PACKET_SIZE)
	{
		ftp_buffer.put(SI->DataBuf.buf);
		SI->DataBuf.len = FTP_PACKET_SIZE;
		TriggerEvent(SI->CompletedEvent);
	}
	else if (BytesTransferred > 0)
	{
		packet = ftp_buffer.peek();
		if (packet.length() < FTP_PACKET_SIZE)
		{
			packet += SI->DataBuf.buf;
			ftp_buffer.update(packet);
			if (packet.length() == FTP_PACKET_SIZE) {
				//full packet  
				SI->DataBuf.len = FTP_PACKET_SIZE;
				TriggerEvent(SI->CompletedEvent);
			}
			else {
				SI->DataBuf.len = FTP_PACKET_SIZE - packet.length();
			}
		}
	}
	memset(SI->FTP_BUFFER, 0, FTP_PACKET_SIZE);
	SI->DataBuf.buf = SI->FTP_BUFFER;

	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), FTP_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			int temp = WSAGetLastError();
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			return;
		}
	}
}

void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	size_t bytes_read;
	// Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;
	
	if (Error != 0)
	{
		printf("I/O operation failed with error %d\n", Error);
	}

	if (BytesTransferred == 0)
	{
		printf("Closing socket %d\n", SI->Socket);
	}

	if (Error != 0 || BytesTransferred == 1)
	{
		GlobalFree(SI);
		return;
	}

	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	//memset(buffer, 0, FTP_PACKET_SIZE);

	if (!feof(requested_file))
	{
		memset(buf, 0, strlen(buf));
		bytes_read = fread(buf, 1, FTP_PACKET_SIZE, requested_file);

		SI->DataBuf.buf = buf;
		SI->DataBuf.len = FTP_PACKET_SIZE;
		if (WSASend(SI->Socket, &(SI->DataBuf), 1, &SendBytes, 0,
			&(SI->Overlapped), FTP_SendRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("WSASend() failed with error %d\n", WSAGetLastError());
				return;
			}
		}
	}
	else 
	{
		// Need to implement file transfer completed protocol
		SI->DataBuf.buf = end_ftp_buf;
		SI->DataBuf.len = 1;
		if (WSASend(SI->Socket, &(SI->DataBuf), 1, &SendBytes, 0,
			&(SI->Overlapped), NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("WSASend() failed with error %d\n", WSAGetLastError());
				return;
			}
		}
	}
}