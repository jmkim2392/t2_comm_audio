#include "multicast.h"

DWORD WINAPI broadcast_data(LPVOID lp) {
	BROADCAST_INFO bi = *(LPBROADCAST_INFO)lp;
	char *file_stream_buf;
	int bytes_read;
	FILE* fp;
	DWORD SendBytes;
	int numAudioSent = 0;

	file_stream_buf = (char*)malloc(AUDIO_PACKET_SIZE);
	bi.DataBuf.buf = bi.AUDIO_BUFFER;

	for (int i = 0; i < LOOP_SEND; i++) {
		if (!fopen_s(&fp, "koto.wav", "rb") == 0) {
			update_server_msgs("Failed opening file to multicast");
		}
	
		while (!feof(fp)) {
			memset(file_stream_buf, 0, AUDIO_PACKET_SIZE);
			bytes_read = fread(file_stream_buf, 1, AUDIO_PACKET_SIZE, fp);
			memcpy(bi.DataBuf.buf, file_stream_buf, bytes_read);
			bi.DataBuf.len = (ULONG)bytes_read;
			if (WSASendTo(*bi.hSocket, &bi.DataBuf, 1, &SendBytes, 0, (struct sockaddr*)bi.stDstAddr, sizeof(*bi.stDstAddr), 0, 0) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					return FALSE;
				}
			}
			writeToAudioBuffer(bi.AUDIO_BUFFER, AUDIO_PACKET_SIZE);
			if (++numAudioSent >= 10) 
			{
				// pass an SvrSendNextAudioEvent to this thread and wait for it here
				WaitForSingleObject(bi.SendNextEvent, INFINITE);
				ResetEvent(bi.SendNextEvent);
				numAudioSent = 1;
			}
		}
		fclose(fp);
	}
	
	free(file_stream_buf);
	return TRUE;
}

DWORD WINAPI receive_data(LPVOID lp) {
	DWORD Flags = 0;
	DWORD RecvBytes;
	SOCKADDR_IN stLclAddr, stSrcAddr;
	struct ip_mreq stMreq;         /* Multicast interface structure */
	SOCKET hSocket;
	WSADATA stWSAData;
	char achMCAddr[MAXADDRSTR] = TIMECAST_ADDR;
	u_short nPort = TIMECAST_PORT;
	LPBROADCAST_INFO bi;

	if (!init_winsock(&stWSAData)) {
		return false;
	}
	if (!get_datagram_socket(&hSocket)) {
		WSACleanup();
		return false;
	}
	if (!bind_socket(&stLclAddr, &hSocket, nPort)) {
		closesocket(hSocket);
		WSACleanup();
		return false;
	}
	if (!set_socket_option_reuseaddr(&hSocket)) {
		closesocket(hSocket);
		WSACleanup();
		return false;
	}
	if (!join_multicast_group(&stMreq, &hSocket, achMCAddr)) {
		closesocket(hSocket);
		WSACleanup();
		return false;
	}

	if ((bi = (LPBROADCAST_INFO)GlobalAlloc(GPTR, sizeof(BROADCAST_INFO))) == NULL) {
		OutputDebugString(L"GlobalAlloc() failed with error\n");
		closesocket(hSocket);
		WSACleanup();
		return false;
	}

	ZeroMemory(&(bi->overlapped), sizeof(WSAOVERLAPPED));
	bi->BytesRECV = 0;
	bi->BytesSEND = 0;
	bi->DataBuf.len = AUDIO_PACKET_SIZE;
	bi->DataBuf.buf = bi->AUDIO_BUFFER;
	bi->stSrcAddr = &stSrcAddr;
	bi->hSocket = &hSocket;

	Flags = 0;

	int addr_size = sizeof(struct sockaddr_in);
	if (WSARecv(*bi->hSocket, &(bi->DataBuf), 1, &RecvBytes, &Flags, &(bi->overlapped), multicast_receive_audio) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			OutputDebugString(L"WSARecvFrom() failed\n");
			*(bool*)lp = false;
		}
	}
	while (*(bool*)lp) {
		SleepEx(5000, TRUE);
	}

	OutputDebugString(L"Exit\n");

	if (!leave_multicast_group(&stMreq, &hSocket, achMCAddr)) {
		OutputDebugString(L"Leave_multicast_group failed\n");
		closesocket(hSocket);
		WSACleanup();
		GlobalFree(bi);
		return false;
	}
	/* Close the socket */
	closesocket(hSocket);
	GlobalFree(bi);
	return TRUE;
}

void CALLBACK multicast_receive_audio(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;

	OutputDebugString(L"Playing\n");
	LPBROADCAST_INFO bi = (LPBROADCAST_INFO)Overlapped;

	Flags = 0;
	ZeroMemory(&(bi->overlapped), sizeof(WSAOVERLAPPED));

	bi->DataBuf.len = AUDIO_PACKET_SIZE;
	bi->DataBuf.buf = bi->AUDIO_BUFFER;

	int addr_size = sizeof(struct sockaddr_in);
	writeToAudioBuffer(bi->DataBuf.buf, AUDIO_BLOCK_SIZE);

	if (WSARecv(*(bi->hSocket), &(bi->DataBuf), 1, &RecvBytes, &Flags, &(bi->overlapped), multicast_receive_audio) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			OutputDebugString(L"WSARecvFrom() failed with error\n");
			return;
		}
	}
}

bool init_winsock(WSADATA *stWSAData) {
	int nRet;
	if (nRet = WSAStartup(0x0202, &(*stWSAData))) {
		OutputDebugString(L"WSAStartup failed\n");
		return false;
	}
	return true;
}

bool get_datagram_socket(SOCKET *hSocket) {
	*hSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (*hSocket == INVALID_SOCKET) {
		OutputDebugString(L"socket() failed, Err\n");
		return false;
	}
	return true;
}

bool bind_socket(SOCKADDR_IN *stLclAddr, SOCKET *hSocket, u_short nPort) {
	stLclAddr->sin_family = AF_INET;
	stLclAddr->sin_addr.s_addr = htonl(INADDR_ANY); /* any interface */
	stLclAddr->sin_port = htons(nPort);                 /* any port */
	if (bind(*hSocket, (struct sockaddr*) &(*stLclAddr), sizeof(*stLclAddr)) == SOCKET_ERROR) {
		OutputDebugString(L"bind() port: %d failed\n");
		return false;
	}
	return true;
}

bool join_multicast_group(struct ip_mreq *stMreq, SOCKET *hSocket, char *achMCAddr) {
	stMreq->imr_multiaddr.s_addr = inet_addr(achMCAddr);
	stMreq->imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(*hSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&(*stMreq), sizeof(*stMreq)) == SOCKET_ERROR) {
		OutputDebugString(L"setsockopt() IP_ADD_MEMBERSHIP address failed\n");
		return false;
	}
	return true;
}

bool set_ip_ttl(SOCKET *hSocket, u_long  lTTL) {
	if (setsockopt(*hSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&lTTL, sizeof(lTTL)) == SOCKET_ERROR) {
		OutputDebugString(L"setsockopt() IP_MULTICAST_TTL failed\n");
		return false;
	}
	return true;
}

bool disable_loopback(SOCKET *hSocket) {
	BOOL fFlag = false;
	if (setsockopt(*hSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&fFlag, sizeof(fFlag)) == SOCKET_ERROR) {
		OutputDebugString(L"setsockopt() IP_MULTICAST_LOOP failed\n");
		return false;
	}
	return true;
}


bool set_socket_option_reuseaddr(SOCKET *hSocket) {
	BOOL fFlag = TRUE;
	if (setsockopt(*hSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&fFlag, sizeof(fFlag)) == SOCKET_ERROR) {
		OutputDebugString(L"setsockopt() SO_REUSEADDR failed\n");
		return false;
	}
	return true;
}


bool leave_multicast_group(struct ip_mreq *stMreq, SOCKET *hSocket, char *achMCAddr) {
	stMreq->imr_multiaddr.s_addr = inet_addr(achMCAddr);
	stMreq->imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(*hSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&stMreq, sizeof(stMreq)) == SOCKET_ERROR) {
		OutputDebugString(L"setsockopt() IP_DROP_MEMBERSHIP failed\n");
		return false;
	}
	return true;
}