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

// �޼����� �޴��Լ�
DWORD WINAPI ThreadReceive(LPVOID pParam)
{
	SOCKET hSocket = (SOCKET)pParam;
	char szBuffer[128] = { 0 };
	while (::recv(hSocket, szBuffer, sizeof(szBuffer), 0) > 0)
	{
		cout << szBuffer << endl;
		memset(szBuffer, 0, sizeof(szBuffer));
	}

	puts("���� �����尡 �������ϴ�.");
	return 0;
}

int main()
{
	// �����ʱ�ȭ
	WSADATA wsa = {};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("�����ʱ�ȭ����");
	}

	// ���ϸ����(Ŭ��)
	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == hSocket)
	{
		HandleError("���ϸ�������");
	}

	// ��ƮĿ��Ʈ (�����ʼ��ϰ� �����Ѵ�)
	SOCKADDR_IN server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(25000);
	server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.0.43");
	if (
		::connect(hSocket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR
		)
	{
		HandleError("�������");
	}

	// �������ͽ���
	cout << "ä�ù� ����" << endl;
	char szBuffer[128]{};
	// �ϴ� �������� ID���ο��޴´�
	::recv(hSocket, szBuffer, sizeof(szBuffer), 0);
	::memcpy(&g_myID, szBuffer, sizeof(int));
	// �޼����� ���� �����带�����
	//ä�� �޽��� ���� ������ ����
	DWORD dwThreadID = 0;
	HANDLE hThread = ::CreateThread(NULL,	
		0,					
		ThreadReceive,		
		(LPVOID)hSocket,	
		0,					
		&dwThreadID);		
	::CloseHandle(hThread);
	
	// ä�ý���
	
	while (true)
	{
		memset(szBuffer, 0, sizeof(szBuffer));
		gets_s(szBuffer);
		if (strcmp(szBuffer, "EXIT") == 0)		break;
		const string chat = format("ID {}: ", g_myID) + szBuffer;
		//����ڰ� �Է��� ���ڿ��� ������ �����Ѵ�.
		if (::send(hSocket, chat.data(), (int)(chat.length() + 1), 0)<=0)
		{
			cout << "�������� ä������" << endl;
			break;
		}
	}

	//������ �ݰ� ����.
	::closesocket(hSocket);
	//�����尡 ����� �� �ֵ��� ��� ��ٷ��ش�.
	::Sleep(100);
	//���� ����
	::WSACleanup();
	
}