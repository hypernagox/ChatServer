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
	
	cout << "�� Ŭ���̾�Ʈ ����" << endl;

	// Ŭ��κ��� ���ڿ��� ����
	while ((nReceive = ::recv(
		hClient, szBuffer.data(), (int)szBuffer.size(), 0
	)) > 0)
	{
		// �״�� Ŭ�� �ٽ� ����
		::send(hClient, szBuffer.data(), (int)szBuffer.size(), 0);
		cout << szBuffer.data() << endl;
		::memset(szBuffer.data(), 0, (int)szBuffer.size());
	}
	cout << "Ŭ��� ���� ����" << endl;
	::closesocket(hClient);
	return 0;
}

int main()
{
	// ���� �ʱ�ȭ
	WSADATA wsa = {0};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("���� �ʱ�ȭ ����");
	}

	// 1. ���� ��� ������ �����Ѵ�
	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
	{
		HandleError("���ϻ�������");
	}

	// 2. ��Ʈ ���ε�
	SOCKADDR_IN svraddr = {0};
	svraddr.sin_family = AF_INET;
	svraddr.sin_port = htons(25000);
	svraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	if (::bind(
		hSocket, (SOCKADDR*)&svraddr, sizeof(svraddr)) == SOCKET_ERROR)
	{
		HandleError("���ε� ����");
	}

	// 3. ���Ӵ�� ���·�
	if (::listen(hSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		HandleError("�����������Խ���");
	}

	// 4. Ŭ���̾�Ʈ ����ó�� �� ����
	SOCKADDR_IN clientaddr = {0};
	int nAddrLen = sizeof(clientaddr);
	SOCKET hClient = 0;
	DWORD dwThreadID = 0;
	HANDLE hThread = {};

	// 4.1 Ŭ���̾�Ʈ ������ ������ ����(����)
	cout << "Ŭ�� ��ٸ�������" << endl;
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
	//5. �������ϴ���
	::closesocket(hSocket);

	//��������
	::WSACleanup();
}