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

	// ������ �ɼ�������
	// FIONBIO << �ϴ� �̰� ���� ���ŷ/����ŷ ����
	// u_long ���� 1�̸� ����ŷ��
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
		// �� �������� �о����
		FD_ZERO(&reads);
		FD_ZERO(&writes);

		// ���� ���� ���� ��Ʈ
		FD_SET(listenSocket, &reads);

		for (auto& s : sessions)
		{
			// ���ú� ����Ʈ�� �� ������ ���ú�� ����Ѵ�
			// ���ú� ����Ʈ�� �� ũ�ٸ� ���� ���� �ؾ��Ұ� �� �����ִܼҸ���
			if (s.recvBytes <= s.sendBytes)
				FD_SET(s.socket, &reads);
			else
				FD_SET(s.socket, &writes);
		}

		// ��� �ϳ� �̻��� ���� �̺�Ʈ�� ��Ʈ�Ǹ� �����ڴ� ������ ������ ������ �������̴´�
		int val = ::select(0, &reads, &writes, nullptr, nullptr);//Ÿ�Ӿƿ���������
		if (val == SOCKET_ERROR)
			return 0;

		// �������Ͽ� �̺�Ʈ���ֳ�
		if (FD_ISSET(listenSocket, &reads))
		{
			// Ŭ���� connect ��û�� �޾���
			SOCKADDR_IN clientAddr;
			int addrLen = sizeof(clientAddr);
			SOCKET clientSocket = ::accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);
			if (clientSocket != INVALID_SOCKET)
			{
				cout << "Client Connected" << endl;
				sessions.push_back(Session{ clientSocket });
			}
		}

		// �������� ���� ������ üũ
		for (auto& s : sessions)
		{
			// ����?
			if (FD_ISSET(s.socket, &reads))
			{
				int recvLen = ::recv(s.socket, s.recvBuffer, BUFSIZE, 0);
				if (recvLen <= 0)
				{
					// TODO : sessions ����
					continue;
				}
				// ���������� ũ�� ���.
				s.recvBytes = recvLen;
			}
			// ����Ʈ?
			if (FD_ISSET(s.socket, &writes))
			{
				// ���ŷ��� -> ������ �ٺ���
				// ����ŷ��� -> �Ϻθ� ���� ���ɼ� ���� (����Ȯ��)
				int sendLen = ::send(s.socket, &s.recvBuffer[s.sendBytes], s.recvBytes - s.sendBytes, 0);
				// ���� ���ݱ��� ���� ����Ʈ�����º��ͽ����ؼ�, �Ѻ������Ҿ� - ���ݱ��������� ��ŭ �ٽú����� .

				if (sendLen == SOCKET_ERROR)
				{
					// TODO : sessions ����
					continue;
				}

				s.sendBytes += sendLen; // 0���� ������ ���� ����Ʈ��ŭ
				// ���� �����Ϳ� ���� �� ���� �����Ͱ� ���ٸ� ������
				if (s.recvBytes == s.sendBytes)
				{
					s.recvBytes = 0;
					s.sendBytes = 0;
				}
			}
		}
	}

	// ���� ����
	::WSACleanup();
}