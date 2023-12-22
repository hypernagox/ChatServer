#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <string>
#include <string_view>
#include <format>
#pragma comment(lib, "ws2_32")
using namespace std;

int g_myID = 0;

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

// 메세지를 받는함수
DWORD WINAPI ThreadReceive(LPVOID pParam)
{
	SOCKET hSocket = (SOCKET)pParam;
	char szBuffer[128] = { 0 };
	while (::recv(hSocket, szBuffer, sizeof(szBuffer), 0) > 0)
	{
		cout << szBuffer << endl;
		memset(szBuffer, 0, sizeof(szBuffer));
	}

	puts("수신 스레드가 끝났습니다.");
	return 0;
}

int main()
{
	// 윈속초기화
	WSADATA wsa = {};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("윈속초기화실패");
	}

	// 소켓만들기(클라)
	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == hSocket)
	{
		HandleError("소켓만들기실패");
	}

	// 포트커넥트 (서버쪽소켓과 연결한다)
	SOCKADDR_IN server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(25000);
	server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.0.43");
	if (
		::connect(hSocket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR
		)
	{
		HandleError("연결실패");
	}

	// 이제부터시작
	cout << "채팅방 입장" << endl;
	char szBuffer[128]{};
	// 일단 서버에게 ID를부여받는다
	::recv(hSocket, szBuffer, sizeof(szBuffer), 0);
	::memcpy(&g_myID, szBuffer, sizeof(int));
	// 메세지를 받을 스레드를만든다
	//채팅 메시지 수신 스레드 생성
	DWORD dwThreadID = 0;
	HANDLE hThread = ::CreateThread(NULL,	
		0,					
		ThreadReceive,		
		(LPVOID)hSocket,	
		0,					
		&dwThreadID);		
	::CloseHandle(hThread);
	
	// 채팅시작
	
	while (true)
	{
		memset(szBuffer, 0, sizeof(szBuffer));
		gets_s(szBuffer);
		if (strcmp(szBuffer, "EXIT") == 0)		break;
		const string chat = format("ID {}: ", g_myID) + szBuffer;
		//사용자가 입력한 문자열을 서버에 전송한다.
		if (::send(hSocket, chat.data(), (int)(chat.length() + 1), 0)<=0)
		{
			cout << "서버에서 채팅종료" << endl;
			break;
		}
	}

	//소켓을 닫고 종료.
	::closesocket(hSocket);
	//스레드가 종료될 수 있도록 잠시 기다려준다.
	::Sleep(100);
	//윈속 해제
	::WSACleanup();
	
}