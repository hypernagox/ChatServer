#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <string_view>
#include <vector>
#pragma comment(lib, "ws2_32")
#include <tchar.h>
#include <Mswsock.h>
using namespace std;

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

struct MY_FILE_DATA
{
	char szName[_MAX_FNAME];
	DWORD dwSize;
};

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

	// 받을 파일이름과 크기에대한 구조체정보를 먼저 받아야함
	MY_FILE_DATA fData = {};
	if (::recv(hSocket, (char*)&fData, sizeof(fData), 0) < sizeof(fData))HandleError("구조체못받음");

	// 파일수신시작
	// 일단 서버로부터온 파일 이름을 토대로 공갈파일하나만듬

	cout << "파일수신시작" << endl;

	HANDLE hFile = ::CreateFileA(
		fData.szName,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,	//파일을 생성한다.
		0,
		NULL);
	if (INVALID_HANDLE_VALUE == hFile)HandleError("파일생성실패");
	char byBuffer[65536];
	DWORD dwTotalRecv = 0, dwRead = 0;
	while (dwTotalRecv < fData.dwSize)
	{
		// 파일내용을 수신하고 길이를 계속잼
		if (const int nRecv = ::recv(hSocket, byBuffer, 65536, 0);
			nRecv > 0)
		{
			dwTotalRecv += nRecv;
			// 서버에서 받은 크기만큼 파일에다가 쓴다.
			::WriteFile(hFile, byBuffer, nRecv, &dwRead, NULL);
			printf("Receive: %d/%d\n", dwTotalRecv, fData.dwSize);
			fflush(stdout);
		}
		else
		{
			cout << "오류발생" << endl;
			break;
		}
	}
	::CloseHandle(hFile);
	::closesocket(hSocket);
	printf("*** 파일수신이 끝났습니다. ***\n");
	::WSACleanup();
}