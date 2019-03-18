#include "ftp_handler.h"

using namespace std;

ifstream fin;
ofstream fout;
DWORD SendBytes;
LPSOCKET_INFORMATION SocketInfo;
BOOL isReceivingFile = FALSE;

char end_ftp_buf[1];
char buffer[FTP_PACKET_SIZE];
circular_buffer<std::string> ftp_buffer(10);

void initialize_ftp(SOCKET* socket, WSAEVENT ftp_packet_recved) {

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
	if (ftp_packet_recved != NULL) {
		SocketInfo->CompletedEvent = ftp_packet_recved;
	}
}

void read_file(std::string filename) {

	fin.open(filename, ios::in | ios::binary);
}

void create_new_file(std::string filename) {
	fout.open(filename, ios::out | ios::binary);
}

void write_file(std::string data) {

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

void start_receiving_file() {
	DWORD RecvBytes;
	DWORD Flags = 0;

	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	if (WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), FTP_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			return;
		}
	}

}

DWORD WINAPI ReceiveFile(LPVOID lpParameter)
{
	std::string dataPacket;
	DWORD Index;
	WSAEVENT EventArray[1];
	LPTCP_SOCKET_INFO tcp_sock_info = (LPTCP_SOCKET_INFO)lpParameter;


	isReceivingFile = TRUE;
	EventArray[0] = tcp_sock_info->CompleteEvent;



	while (isReceivingFile)
	{
		Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
		WSAResetEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

		dataPacket = ftp_buffer.get();

		if (!dataPacket.empty()) {
			write_file(dataPacket);
		}
	}
	return 0;
}

void CALLBACK FTP_ReceiveRoutine(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;
	CHAR ftp_packet_buf[FTP_PACKET_SIZE];
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
			SI->DataBuf.len = FTP_PACKET_SIZE - BytesTransferred;
		}

		if (packet.length() == FTP_PACKET_SIZE)
		{
			//full packet  
			SI->DataBuf.len = FTP_PACKET_SIZE;
			TriggerEvent(SI->CompletedEvent);
		}
	}

	SI->DataBuf.buf = ftp_packet_buf;

	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), FTP_ReceiveRoutine) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			return;
		}
	}
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