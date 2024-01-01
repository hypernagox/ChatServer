#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <atomic>
#include <mutex>
#include <windows.h>
#include <future>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

void HandleError(const char* cause)
{
	int errCode = ::WSAGetLastError();
	cout << cause << " ErrorCode : " << errCode << endl;
}

const int BUFSIZE = 1000;

struct Session
{
	SOCKET socket = INVALID_SOCKET;
	char recvBuffer[BUFSIZE] = {};
	int recvBytes = 0;
	int sendBytes = 0;
};

int main()
{
	WSAData wsaData;
	if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return 0;

	SOCKET listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket == INVALID_SOCKET)
		return 0;

	// 소켓의 옵션을지정
	// FIONBIO << 일단 이걸 쓰면 블로킹/논블로킹 선택
	// u_long 값이 1이면 논블로킹임
	u_long on = 1;
	if (::ioctlsocket(listenSocket, FIONBIO, &on) == INVALID_SOCKET)
		return 0;

	SOCKADDR_IN serverAddr;
	::memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ::htonl(INADDR_ANY);
	serverAddr.sin_port = ::htons(7777);

	if (::bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		return 0;

	if (::listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
		return 0;

	cout << "Accept" << endl;

	vector<Session> sessions;
	sessions.reserve(100);

	fd_set reads;
	fd_set writes;

	while (true)
	{
		// 매 루프마다 밀어야함
		FD_ZERO(&reads);
		FD_ZERO(&writes);

		// 서버 리슨 소켓 세트
		FD_SET(listenSocket, &reads);

		for (auto& s : sessions)
		{
			// 리시브 바이트가 더 적으면 리시브로 등록한다
			// 리시브 바이트가 더 크다면 아직 센드 해야할게 더 남아있단소리임
			if (s.recvBytes <= s.sendBytes)
				FD_SET(s.socket, &reads);
			else
				FD_SET(s.socket, &writes);
		}

		// 적어도 하나 이상의 소켓 이벤트가 세트되면 낙오자는 버리고 리턴함 무조건 성공만뽑는다
		int val = ::select(0, &reads, &writes, nullptr, nullptr);//타임아웃설정가능
		if (val == SOCKET_ERROR)
			return 0;

		// 리슨소켓에 이벤트가있나
		if (FD_ISSET(listenSocket, &reads))
		{
			// 클라의 connect 요청을 받아줌
			SOCKADDR_IN clientAddr;
			int addrLen = sizeof(clientAddr);
			SOCKET clientSocket = ::accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);
			if (clientSocket != INVALID_SOCKET)
			{
				cout << "Client Connected" << endl;
				sessions.push_back(Session{ clientSocket });
			}
		}

		// 리슨소켓 말고 나머지 체크
		for (auto& s : sessions)
		{
			// 리드?
			if (FD_ISSET(s.socket, &reads))
			{
				int recvLen = ::recv(s.socket, s.recvBuffer, BUFSIZE, 0);
				if (recvLen <= 0)
				{
					// TODO : sessions 제거
					continue;
				}
				// 받은데이터 크기 기억.
				s.recvBytes = recvLen;
			}
			// 라이트?
			if (FD_ISSET(s.socket, &writes))
			{
				// 블로킹모드 -> 무조건 다보냄
				// 논블로킹모드 -> 일부만 보낼 가능성 있음 (낮은확률)
				int sendLen = ::send(s.socket, &s.recvBuffer[s.sendBytes], s.recvBytes - s.sendBytes, 0);
				// 내가 지금까지 보낸 바이트오프셋부터시작해서, 총보내야할양 - 지금까지보낸양 만큼 다시보낸다 .

				if (sendLen == SOCKET_ERROR)
				{
					// TODO : sessions 제거
					continue;
				}

				s.sendBytes += sendLen; // 0에서 실제로 더한 바이트만큼
				// 받은 데이터와 내가 총 보낸 데이터가 같다면 리셋함
				if (s.recvBytes == s.sendBytes)
				{
					s.recvBytes = 0;
					s.sendBytes = 0;
				}
			}
		}
	}

	// 윈속 종료
	::WSACleanup();
}