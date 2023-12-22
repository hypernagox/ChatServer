#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <string>
#include <string_view>
#pragma comment(lib, "ws2_32")
using namespace std;

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

int main()
{
	// 윈속 초기화
	WSADATA wsa = {};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("윈속초기화실패");
	}

	// 1. 접속대기 소켓생성
	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
	{
		HandleError("소켓생성실패");
	}

	// 2. 포트 바인딩
	SOCKADDR_IN svraddr = {};
	svraddr.sin_family = AF_INET;
	svraddr.sin_port = htons(25000);
	svraddr.sin_addr.S_un.S_addr = inet_addr("192.168.0.43");

	if (::connect(hSocket,
		(SOCKADDR*)&svraddr, sizeof(svraddr)) == SOCKET_ERROR)
	{
		HandleError("연결실패");
	}

	// 3. 메세지 송 수신
	char szBuffer[128] = {};
	while (true)
	{
		cin >> szBuffer;
		if (strcmp(szBuffer, "EXIT") == 0)break;
		
		// 입력한 문자를 보낸다
		::send(hSocket, szBuffer, (int)(strlen(szBuffer) + 1), 0);
		::memset(szBuffer, 0, sizeof(szBuffer));
		// 받는다
		::recv(hSocket, szBuffer, sizeof(szBuffer), 0);
		printf("From server: %s\n", szBuffer);
	}

}