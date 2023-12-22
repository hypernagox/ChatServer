#include <winsock2.h>
#include <iostream>
#include <string_view>
#include <vector>
#pragma comment(lib, "ws2_32")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

DWORD AcceptClient(LPVOID pParam)
{
	vector<char> szBuffer(128);
	
	int nReceive = 0;
	SOCKET hClient = (SOCKET)pParam;
	
	cout << "새 클라이언트 연결" << endl;

	// 클라로부터 문자열을 수신
	while ((nReceive = ::recv(
		hClient, szBuffer.data(), (int)szBuffer.size(), 0
	)) > 0)
	{
		// 그대로 클라에 다시 전송
		::send(hClient, szBuffer.data(), (int)szBuffer.size(), 0);
		cout << szBuffer.data() << endl;
		::memset(szBuffer.data(), 0, (int)szBuffer.size());
	}
	cout << "클라와 연결 해제" << endl;
	::closesocket(hClient);
	return 0;
}

int main()
{
	// 윈속 초기화
	WSADATA wsa = {0};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("윈속 초기화 실패");
	}

	// 1. 접속 대기 소켓을 생성한다
	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
	{
		HandleError("소켓생성실패");
	}

	// 2. 포트 바인딩
	SOCKADDR_IN svraddr = {0};
	svraddr.sin_family = AF_INET;
	svraddr.sin_port = htons(25000);
	svraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	if (::bind(
		hSocket, (SOCKADDR*)&svraddr, sizeof(svraddr)) == SOCKET_ERROR)
	{
		HandleError("바인딩 실패");
	}

	// 3. 접속대기 상태로
	if (::listen(hSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		HandleError("리슨상태진입실패");
	}

	// 4. 클라이언트 접속처리 및 대응
	SOCKADDR_IN clientaddr = {0};
	int nAddrLen = sizeof(clientaddr);
	SOCKET hClient = 0;
	DWORD dwThreadID = 0;
	HANDLE hThread = {};

	// 4.1 클라이언트 연결후 새소켓 생성(개방)
	cout << "클라를 기다리고있음" << endl;
	while ((hClient = ::accept(hSocket, (SOCKADDR*)&clientaddr, &nAddrLen)) != INVALID_SOCKET)
	{
		hThread = ::CreateThread(
			NULL,
			0,
			AcceptClient,
			(LPVOID)hClient,
			0,
			&dwThreadID
		);
		::CloseHandle(hThread);
	}
	//5. 리슨소켓닫음
	::closesocket(hSocket);

	//윈속해제
	::WSACleanup();
}