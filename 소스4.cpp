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
	// ���� �ʱ�ȭ
	WSADATA wsa = { 0 };
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
	SOCKADDR_IN svraddr = { 0 };
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

	// 4. Ŭ���̾�Ʈ ����ó�� �� ���� (Ŭ���� ����)
	// 4.1 Ŭ���̾�Ʈ ������ ������ ����(����)

	cout << "Ŭ�� ��ٸ�������" << endl;
	SOCKADDR_IN clientaddr = { 0 };
	int nAddrLen = sizeof(clientaddr);
	SOCKET hClient = ::accept(hSocket, (SOCKADDR*)&clientaddr, &nAddrLen);
	if (INVALID_SOCKET == hClient)
	{
		HandleError("Ŭ����޾���");
	}
	cout << "Ŭ�����ӿϷ�" << endl;
	
	//������ ������ ������ �غ�

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

	// ������ ����
	if (
		::TransmitFile(
			hClient, // ���۹��� Ŭ�� ����
			hFile, // �����ڵ�
			0, // ����ũ�� �ٵ� 0�̸�˾Ƽ� ����
			65536, // �ѹ��� ������ ���� ũ��
			NULL, // �񵿱� ����¿� ����� �������� ����ü
			&tfb, // ���� �����ϱ����� ���� ���� ������
			0 //��Ÿ
		) == FALSE
		)HandleError("�������� ����");

	// Ŭ����� ������½�ȣ�� ��ٸ�
	::recv(hClient, NULL, 0, 0);

	::closesocket(hClient);
	::closesocket(hSocket);
	::CloseHandle(hFile);
	::WSACleanup();
}