#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <string_view>
#include <vector>
#pragma comment(lib, "ws2_32")
#include <tchar.h>
#include <Mswsock.h>
#pragma comment(lib, "Mswsock")

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
	// 윈속 초기화
	WSADATA wsa = { 0 };
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
	SOCKADDR_IN svraddr = { 0 };
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

	// 4. 클라이언트 접속처리 및 대응 (클라의 소켓)
	// 4.1 클라이언트 연결후 새소켓 생성(개방)

	cout << "클라를 기다리고있음" << endl;
	SOCKADDR_IN clientaddr = { 0 };
	int nAddrLen = sizeof(clientaddr);
	SOCKET hClient = ::accept(hSocket, (SOCKADDR*)&clientaddr, &nAddrLen);
	if (INVALID_SOCKET == hClient)
	{
		HandleError("클라못받았음");
	}
	cout << "클라접속완료" << endl;
	
	//전송할 파일의 정보를 준비

	HANDLE hFile = ::CreateFile(_T("Sleep Away.zip"),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);

	MY_FILE_DATA fData = { "Sleep Away.zip",0 };
	fData.dwSize = GetFileSize(hFile, NULL);
	TRANSMIT_FILE_BUFFERS tfb = {};
	tfb.Head = &fData;
	tfb.HeadLength = sizeof(fData);

	// 파일을 보냄
	if (
		::TransmitFile(
			hClient, // 전송받을 클라 소켓
			hFile, // 파일핸들
			0, // 파일크기 근데 0이면알아서 맞춤
			65536, // 한번에 전송할 버퍼 크기
			NULL, // 비동기 입출력에 사용할 오버랩드 구조체
			&tfb, // 파일 전송하기전에 먼저 보낼 데이터
			0 //기타
		) == FALSE
		)HandleError("파일전송 실패");

	// 클라놈이 연결끊는신호를 기다림
	::recv(hClient, NULL, 0, 0);

	::closesocket(hClient);
	::closesocket(hSocket);
	::CloseHandle(hFile);
	::WSACleanup();
}