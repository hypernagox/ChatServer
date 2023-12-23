#include <winsock2.h>
#include <iostream>
#include <string_view>
#include <vector>
#include <mutex>
#include <format>
#include <algorithm>
#pragma comment(lib, "ws2_32")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;

mutex g_mt;
vector<SOCKET> g_vecClients;
int g_clientID = 0;
SOCKET hSocket;

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

void SendMessageAll(string_view client_msg,const int len)
{
	const int msg_len = (int)client_msg.length();

	// ���� Ŭ�� ����/�����°�+ ����������޼��� ���� ���
	{
		lock_guard<mutex> lock{ g_mt };
		for (const auto& client : g_vecClients)
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
//	cout << "Ŭ�� ���� ID: " << client_ID << endl;
//
//	// �ϴ� �����κ��� ���°� ������
//	while (
//		(nReceive = ::recv(hClient, szBuffer.data(), (int)szBuffer.size(), 0)) > 0
//		)
//	{
//		// �ϴ� Ŭ�� ���Ѱ� ģ��
//		cout << szBuffer.data() << endl;
//		// ��ü Ŭ������ �����Ѵ�
//		SendMessageAll(szBuffer.data());
//		::memset(szBuffer.data(), 0, (int)szBuffer.size());
//	}
//
//	cout << format("ID{} ��������", client_ID) << endl;
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
			for (const auto& client : g_vecClients)
			{
				::closesocket(client);
			}
			g_vecClients.clear();
		}

		puts("��� Ŭ���̾�Ʈ ������ �����߽��ϴ�.");
		//Ŭ���̾�Ʈ�� ����ϴ� ��������� ����Ǳ⸦ ��ٸ���.
		::Sleep(100);
		::closesocket(hSocket);

		//���� ����
		::WSACleanup();
		exit(0);
		return TRUE;
	}

	return FALSE;
}

int main()
{
	// ���� �ʱ�ȭ
	WSADATA wsa = {};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("���� �ʱ�ȭ ����");
	}

	// �̺�Ʈ���
	if (::SetConsoleCtrlHandler(
		(PHANDLER_ROUTINE)CtrlHandler, TRUE) == FALSE)
		puts("ERROR: Ctrl+C ó���⸦ ����� �� �����ϴ�.");

	// ���Ӵ�� ���ϻ���
	hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == hSocket)
	{
		HandleError("���ϻ�������");
	}
	// ��Ʈ���ε�
	SOCKADDR_IN server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(25000);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (::bind(hSocket, (sockaddr*)&server_addr, sizeof(server_addr))
		== SOCKET_ERROR)
	{
		HandleError("���Ϲ��ε�����");
	}

	// Ŭ���� ������ ���� �غ� (�������·ΰ�)
	if (::listen(hSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		HandleError("�����������Խ���");
	}

	cout << "== ä�ü��� ���� ==" << endl;

	// ������ ��ȭ�� ����
	FD_SET fdRead;
	g_vecClients.emplace_back(hSocket);
	while (NULL != hSocket)
	{
		FD_ZERO(&fdRead);
		for (int i = 0; i < (int)g_vecClients.size();)
		{
			auto& socket = g_vecClients[i];
			if (NULL == socket)
			{
				socket = g_vecClients.back();
				g_vecClients.pop_back();
			}
			else
			{
				FD_SET(socket, &fdRead);
				++i;
			}
		}
		// ��ȭ�� ���°��� ��ٸ���
		::select(0, &fdRead, NULL, NULL, NULL);

		for (auto& fd : fdRead.fd_array)
		{
			// �����÷��װ� ��Ʈ�����ʾ����� �ƹ��͵� �����ʴ´�.
			if (!FD_ISSET(fd, &fdRead))
			{
				continue;
			}
			
			// ������ ���Ͽ� �����̿Դٸ� �� Ŭ�󸦿����Ѵ�
			if (fd == hSocket)
			{
				SOCKADDR_IN client_addr = {};
				int nAddrLen = sizeof(client_addr);
				if (SOCKET hClient = ::accept(hSocket, (SOCKADDR*)&client_addr, &nAddrLen);
					hClient != INVALID_SOCKET)
				{
					g_vecClients.emplace_back(hClient);
					++g_clientID;
					::send(hClient, (char*)&g_clientID, sizeof(int), 0);
				}
			}
			// Ŭ����Ͽ� �����̿Դٸ� �޼����� �Դ�.
			else
			{
				vector<char> szBuffer(128);
				const int nReceive = ::recv(fd, szBuffer.data(), (int)szBuffer.size(), 0);
				// ��������
				if (nReceive <= 0)
				{
					::closesocket(fd);
					fd = NULL;
					puts("Ŭ���̾�Ʈ�� ������ �������ϴ�.");
				}
				// �޼�������
				else
				{
					cout << szBuffer.data() << endl;
					SendMessageAll(szBuffer.data(), nReceive+1);
				}
			}
		}
	}

	::shutdown(hSocket, SD_BOTH);

	{
		lock_guard<mutex> lock{ g_mt };
		for (const auto& client : g_vecClients)
		{
			::closesocket(client);
		}
		g_vecClients.clear();
	}

	puts("*** ä�ü����� �����մϴ�. ***");

}