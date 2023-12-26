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

	// ���� �����̸��� ũ�⿡���� ����ü������ ���� �޾ƾ���
	MY_FILE_DATA fData = {};
	if (::recv(hSocket, (char*)&fData, sizeof(fData), 0) < sizeof(fData))HandleError("����ü������");

	// ���ϼ��Ž���
	// �ϴ� �����κ��Ϳ� ���� �̸��� ���� ���������ϳ�����

	cout << "���ϼ��Ž���" << endl;

	HANDLE hFile = ::CreateFileA(
		fData.szName,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,	//������ �����Ѵ�.
		0,
		NULL);
	if (INVALID_HANDLE_VALUE == hFile)HandleError("���ϻ�������");
	char byBuffer[65536];
	DWORD dwTotalRecv = 0, dwRead = 0;
	while (dwTotalRecv < fData.dwSize)
	{
		// ���ϳ����� �����ϰ� ���̸� �����
		if (const int nRecv = ::recv(hSocket, byBuffer, 65536, 0);
			nRecv > 0)
		{
			dwTotalRecv += nRecv;
			// �������� ���� ũ�⸸ŭ ���Ͽ��ٰ� ����.
			::WriteFile(hFile, byBuffer, nRecv, &dwRead, NULL);
			printf("Receive: %d/%d\n", dwTotalRecv, fData.dwSize);
			fflush(stdout);
		}
		else
		{
			cout << "�����߻�" << endl;
			break;
		}
	}
	::CloseHandle(hFile);
	::closesocket(hSocket);
	printf("*** ���ϼ����� �������ϴ�. ***\n");
	::WSACleanup();
}