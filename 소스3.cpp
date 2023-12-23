#include <winsock2.h>
#include <iostream>
#include <string_view>
#include <vector>
#include <mutex>
#include <format>
#include <algorithm>
#include <array>
#pragma comment(lib, "ws2_32")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;

mutex g_mt;
int curIdx = 0;
//array<SOCKET,WSA_MAXIMUM_WAIT_EVENTS> arrSocket;
//array<WSAEVENT, WSA_MAXIMUM_WAIT_EVENTS> arrEvents;
int g_clientID = 0;
SOCKET hSocket;

//소켓에 대한 이벤트 핸들의 배열
WSAEVENT	arrEvents[WSA_MAXIMUM_WAIT_EVENTS];
//소켓의 배열. 이벤트 핸들 배열과 쌍을 이룬다.
SOCKET		arrSocket[WSA_MAXIMUM_WAIT_EVENTS];

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

void SendMessageAll(string_view client_msg, const int len)
{
	const int msg_len = (int)client_msg.length();

	// 도중 클라 입장/나가는거+ 여러스레드메세지 전송 방어
	{
		lock_guard<mutex> lock{ g_mt };
		for (const auto& client : arrSocket)
		{
			::send(client, client_msg.data(), len, 0);
		}
	}
}

//DWORD WINAPI ThreadFunction(LPVOID pParam)
//{
//	vector<char> szBuffer(128);
//	int nReceive = 0;
//	SOCKET hClient = (SOCKET)pParam;
//	const int client_ID = g_clientID;
//
//	cout << "클라 입장 ID: " << client_ID << endl;
//
//	// 일단 서버로부터 오는걸 들어야지
//	while (
//		(nReceive = ::recv(hClient, szBuffer.data(), (int)szBuffer.size(), 0)) > 0
//		)
//	{
//		// 일단 클라가 말한걸 친다
//		cout << szBuffer.data() << endl;
//		// 전체 클라한테 전송한다
//		SendMessageAll(szBuffer.data());
//		::memset(szBuffer.data(), 0, (int)szBuffer.size());
//	}
//
//	cout << format("ID{} 접속종료", client_ID) << endl;
//	{
//		lock_guard<mutex> lock{ g_mt };
//		auto iter = std::ranges::find(g_vecClients, hClient);
//		if (g_vecClients.end() != iter)
//		{
//			*iter = g_vecClients.back();
//			g_vecClients.pop_back();
//		}
//	}
//	return 0;
//}
//
//void AddClient(SOCKET _client, HANDLE callBack)
//{
//	{
//		lock_guard<mutex> lock{ g_mt };
//		g_vecClients.emplace_back(_client);
//	}
//
//	DWORD dwThreadID = 0;
//
//	callBack = ::CreateThread(
//		NULL,
//		0,
//		ThreadFunction,
//		(LPVOID)_client,
//		0,
//		&dwThreadID
//	);
//
//	::CloseHandle(callBack);
//}

BOOL CtrlHandler(DWORD dwType)
{
	if (dwType == CTRL_C_EVENT)
	{

		::shutdown(hSocket, SD_BOTH);

		{
			lock_guard<mutex> lock{ g_mt };
			for (const auto& client : arrSocket)
			{
				::closesocket(client);
			}
			//arrSocket.clear();
		}

		puts("모든 클라이언트 연결을 종료했습니다.");
		//클라이언트와 통신하는 스레드들이 종료되기를 기다린다.
		::Sleep(100);
		::closesocket(hSocket);

		//윈속 해제
		::WSACleanup();
		exit(0);
		return TRUE;
	}

	return FALSE;
}

int main()
{
	// 윈속 초기화
	WSADATA wsa = {};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("윈속 초기화 실패");
	}

	// 이벤트등록
	if (::SetConsoleCtrlHandler(
		(PHANDLER_ROUTINE)CtrlHandler, TRUE) == FALSE)
		puts("ERROR: Ctrl+C 처리기를 등록할 수 없습니다.");

	// 접속대기 소켓생성
	hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == hSocket)
	{
		HandleError("소켓생성실패");
	}
	// 포트바인딩
	SOCKADDR_IN server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(25000);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (::bind(hSocket, (sockaddr*)&server_addr, sizeof(server_addr))
		== SOCKET_ERROR)
	{
		HandleError("소켓바인딩실패");
	}

	// 클라의 접속을 받을 준비 (리슨상태로감)
	if (::listen(hSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		HandleError("리슨상태진입실패");
	}

	cout << "== 채팅서버 시작 ==" << endl;

	// 이벤트 생성 및 등록
	
	arrSocket[curIdx] = hSocket;
	arrEvents[curIdx]= ::WSACreateEvent();

	// 서버소켓에 ACCEPT이벤트를 감지하도록함
	if (::WSAEventSelect(arrSocket[curIdx], arrEvents[curIdx],
		FD_ACCEPT) == SOCKET_ERROR)
	{
		puts("ERROR: WSAEventSelect()");
		return 0;
	}
	// 루프를 돌면서 이벤트를 처리한다.
	DWORD dwIndex;
	WSANETWORKEVENTS netEvent;

	while (true)
	{
		dwIndex = ::WSAWaitForMultipleEvents(
			curIdx + 1, // 감시 할 이벤트 갯수
			arrEvents, // 이벤트 배열
			FALSE, // 전체 대기 X
			100, // 100ms만대기해봄
			FALSE // 스레드상태변경 X
		);

		if (dwIndex == WSA_WAIT_FAILED)
			continue;

		//이벤트가 발생한 소켓의 인덱스 및 이벤트 발생 이유를 확인한다.
		if (::WSAEnumNetworkEvents(arrSocket[dwIndex],
			arrEvents[dwIndex], &netEvent) == SOCKET_ERROR)
			continue;

		const auto eve = netEvent.lNetworkEvents & (-1);
		switch (eve)
		{
			// 클라가오셨다
		case FD_ACCEPT:
		{
			if (netEvent.iErrorCode[FD_ACCEPT_BIT] != 0
				|| curIdx >= WSA_MAXIMUM_WAIT_EVENTS)
			{
				cout << "에러" << endl;
				continue;
			}
			SOCKADDR_IN client_addr = {};
			int nAddrLen = sizeof(client_addr);
			if (SOCKET hClient = ::accept(hSocket, (SOCKADDR*)&client_addr, &nAddrLen);
				hClient != INVALID_SOCKET)
			{
				++curIdx;
				arrSocket[curIdx] = (hClient);
				arrEvents[curIdx] = (::WSACreateEvent());
				++g_clientID;
				::send(hClient, (char*)&g_clientID, sizeof(int), 0);
				cout << "새 클라 연결" << endl;
				::WSAEventSelect(hClient, arrEvents[curIdx],
					FD_READ | FD_CLOSE);
			}
		}
		break;
		//클라가 나갔다
		case FD_CLOSE:
		{
			//클라이언트 소켓 및 이벤트 핸들을 닫는다.
			::WSACloseEvent(arrEvents[dwIndex]);
			::shutdown(arrSocket[dwIndex], SD_BOTH);
			::closesocket(arrSocket[dwIndex]);

			arrSocket[dwIndex] = arrSocket[curIdx];
			arrEvents[dwIndex] = arrEvents[curIdx];
			arrSocket[curIdx] = NULL;
			arrEvents[curIdx] = NULL;

			--curIdx;
			printf("클라이언트 연결 종료. 남은 수: %d.\n", curIdx);
		}
		break;
		case FD_READ:
		{
			vector<char> szBuffer(128);
			const int nReceive = ::recv(arrSocket[dwIndex], szBuffer.data(), (int)szBuffer.size(), 0);
			cout << szBuffer.data() << endl;
			SendMessageAll(szBuffer.data(), nReceive);
		}
		break;
		default:
			break;
		}
	}

	::shutdown(hSocket, SD_BOTH);

	{
		lock_guard<mutex> lock{ g_mt };
		for (const auto& client : arrSocket)
		{
			::closesocket(client);
		}
		//arrSocket.clear();
	}

	puts("*** 채팅서버를 종료합니다. ***");

}