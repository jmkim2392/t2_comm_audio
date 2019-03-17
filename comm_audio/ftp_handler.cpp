#include "ftp_handler.h"

using namespace std;

ifstream fin;
ofstream fout;
DWORD SendBytes;
LPSOCKET_INFORMATION SocketInfo;

char end_ftp_buf[1];
char buffer[FTP_PACKET_SIZE];
circular_buffer<char*> ftp_buffer(10);

void initialize_ftp(SOCKET* socket) {
	
	// Create a socket information structure to associate with the accepted socket.
	if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		printf("GlobalAlloc() failed with error %d\n", GetLastError());
		terminate_connection();
		return;
	}

	// Fill in the details of our accepted socket.
	SocketInfo->Socket = *socket;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	SocketInfo->BytesSEND = 0;
	SocketInfo->BytesRECV = 0;
	SocketInfo->DataBuf.len = DEFAULT_REQUEST_PACKET_SIZE;
	SocketInfo->DataBuf.buf = SocketInfo->Buffer;
}

void read_file(std::string filename) {
	
	fin.open(filename, ios::in | ios::binary);
}

void create_new_file(std::string filename) {
	fout.open(filename, ios::out | ios::binary);
}

void write_file() {

}



void packetize_file() {

}

void start_sending_file() {
	memset(buffer, 0, FTP_PACKET_SIZE);
	fin.read(buffer, FTP_PACKET_SIZE);

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

void receive_file() {

}

void CALLBACK FTP_SendRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
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

	if (Error != 0 || BytesTransferred == 0)
	{
		GlobalFree(SI);
		return;
	}

	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	memset(buffer, 0, FTP_PACKET_SIZE);
	

	

	if (!fin.eof()) {
		fin.read(buffer, FTP_PACKET_SIZE);
		SI->DataBuf.buf = buffer;
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
	else {
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