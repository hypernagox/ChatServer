#include <iostream>
#include <winsock2.h>
#pragma comment(lib, "ws2_32")
#include <windows.h>
#include <list>
#include <iterator>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <algorithm>
#include <string>
#include <string_view>
#include <format>
#include <tchar.h>

using std::list;
using std::vector;
using std::shared_ptr;
using std::unordered_map;

static constexpr int MAX_CLIENTS = 5000;

class IOCPServer;
class Session;

class SessionMgr
{
private:
	list<shared_ptr<Session>> m_listSession;
	unordered_map<Session*, decltype(m_listSession.begin())> m_mapSessionIter;
	std::shared_mutex m_rwLock;
public:
	SessionMgr();
	
	void AddClientSession(shared_ptr<Session> pSession);
	
	void RemoveSession(shared_ptr<Session> pSession);
	
	shared_ptr<Session> FindSession(Session* pSessionKey);
	

	void SendAllMessage(shared_ptr<Session> pSession);
};

shared_ptr<SessionMgr> g_SessionMgr;
shared_ptr<IOCPServer> g_IOCPServer;

struct OVERLAPPED_EX
	:public OVERLAPPED
{
	WSABUF			wsaBuf;
	DWORD			dwTransfer;
	DWORD			dwFlag;
	int offset = 0;
	char	buffer[4096]{};
	enum OperationType { SEND, RECEIVE } operationType;
	OVERLAPPED_EX(const int id)
	{
		const std::wstring strID = std::format(L"ID{}: ", id);
		offset = (int)strID.size() * sizeof(wchar_t); 
		::memcpy(buffer, strID.data(), offset); 
	}
	void ReadyToRecv()
	{
		::ZeroMemory((OVERLAPPED*)this, sizeof(OVERLAPPED));
		dwFlag = 0;
		wsaBuf.buf = buffer + offset;

		dwTransfer = 0;
		wsaBuf.len = sizeof(buffer);
		operationType = RECEIVE;
	}
	void ReadyToSend(const int len,const char* recvBuff)
	{
		::ZeroMemory(this, sizeof(OVERLAPPED_EX));
		dwFlag = 0;
		wsaBuf.buf = buffer;

		wsaBuf.len = dwTransfer = len;
		::memcpy(buffer, recvBuff, len);
		operationType = SEND;
	}
	bool OnRecv(SOCKET hClient)
	{
		ReadyToRecv();
		::WSARecv(hClient, &wsaBuf, 1, &dwTransfer,
			&dwFlag, this, NULL);
		const bool bIsRecv = ::WSAGetLastError() != WSA_IO_PENDING;
		if (bIsRecv)
			puts("ERROR: WSARecv() != WSA_IO_PENDING");
		return bIsRecv;
	}
	bool OnSend(SOCKET hClient)
	{
		return 0 == ::WSASend(hClient, &wsaBuf, 1, &dwTransfer,
			dwFlag, this, NULL);
	}
};

class Session
	:public std::enable_shared_from_this<Session>
{
private:
	SOCKET m_hClientSocket = NULL;
	OVERLAPPED_EX m_stOverlappedRecv;
	OVERLAPPED_EX m_stOverlappedSend;
	int m_iClientID = 0;
public:
	Session(const int _id)
		:m_iClientID{_id}
		,m_stOverlappedRecv{_id}
		,m_stOverlappedSend{_id}
	{
	}

	~Session()
	{
		::shutdown(m_hClientSocket, SD_BOTH);
		::closesocket(m_hClientSocket);
	}

	bool OnAcceptClient(SOCKET hServerSocket,HANDLE iocpHandle)
	{
		SOCKADDR sockAddr;
		int	nAddrSize = sizeof(SOCKADDR);

		m_hClientSocket = ::accept(hServerSocket,
			&sockAddr, &nAddrSize);

		if (INVALID_SOCKET != m_hClientSocket)
		{
			::CreateIoCompletionPort((HANDLE)m_hClientSocket, iocpHandle,
				(ULONG_PTR)this,	
				0);

			const std::wstring temp = std::format(_T("ID{} ����\n"), m_iClientID);
			m_stOverlappedSend.ReadyToSend((int)temp.size() * sizeof(wchar_t), (char*)temp.data());
			g_SessionMgr->SendAllMessage(this->shared_from_this());

			return true;
		}
		else
		{
			return false;
		}
	}

	bool OnRecive()
	{
		return m_stOverlappedRecv.OnRecv(m_hClientSocket);
	}

	bool OnSend(shared_ptr<Session> other)
	{
		return m_stOverlappedSend.OnSend(other->GetClinetSocket());
	}

	SOCKET GetClinetSocket() { return m_hClientSocket; }
	void SetReadyToSend(const int len) {
		m_stOverlappedSend.ReadyToSend(len + m_stOverlappedRecv.offset,m_stOverlappedRecv.buffer);
	}
};

DWORD WINAPI ThreadComplete(LPVOID pParam);

class IOCPServer
{
	static inline int g_cliendID = 0;
private:
	HANDLE m_IOCPHandle;
	SOCKET m_hServerSockket = NULL;
	WSADATA m_wsa;
public:
	void StartIOCP()
	{
		WSADATA wsa = { 0 };
		if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		{
			puts("ERROR: ������ �ʱ�ȭ �� �� �����ϴ�.");
		}

		m_IOCPHandle = ::CreateIoCompletionPort(
			INVALID_HANDLE_VALUE,	//����� ���� ����.
			NULL,			//���� �ڵ� ����.
			0,				//�ĺ���(Key) �ش���� ����.
			0);				//������ ������ OS�� �ñ�.

		if (m_IOCPHandle == NULL)
		{
			puts("ERORR: IOCP�� ������ �� �����ϴ�.");
		}

		
		HANDLE hThread;
		DWORD dwThreadID;
		for (int i = 0; i < 12; ++i)
		{
			dwThreadID = 0;
			//Ŭ���̾�Ʈ�κ��� ���ڿ��� ������.
			hThread = ::CreateThread(NULL,	//���ȼӼ� ���
				0,				//���� �޸𸮴� �⺻ũ��(1MB)
				ThreadComplete,	//������� ������ �Լ��̸�
				(LPVOID)NULL,	//
				0,				//���� �÷��״� �⺻�� ���
				&dwThreadID);	//������ ������ID�� ����� �����ּ�
		
			::CloseHandle(hThread);
		}

		//���� ���� ���� ����
		m_hServerSockket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
			NULL, 0, WSA_FLAG_OVERLAPPED);

		SOCKADDR_IN addrsvr;
		addrsvr.sin_family = AF_INET;
		addrsvr.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY);
		addrsvr.sin_port = ::htons(25000);

		if (::bind(m_hServerSockket,
			(SOCKADDR*)&addrsvr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
		{
			puts("ERROR: ��Ʈ�� �̹� ������Դϴ�.");
		}

		if (::listen(m_hServerSockket, SOMAXCONN) == SOCKET_ERROR)
		{
			puts("ERROR:  ���� ���·� ��ȯ�� �� �����ϴ�.");
		}

		m_wsa = wsa;
	}

	void AcceptClientLoop()
	{
		shared_ptr<Session> pSession = nullptr;
		while (
			(pSession = std::make_shared<Session>(g_cliendID++))->OnAcceptClient(m_hServerSockket,m_IOCPHandle)
			)
		{
			auto tempPtr = pSession.get();
			g_SessionMgr->AddClientSession(std::move(pSession));
			tempPtr->OnRecive();

			std::cout << "Ŭ������" << std::endl;
		}
	}

	auto GetIOCPHandle() { return m_IOCPHandle; }
};

int main()
{
	g_SessionMgr = std::make_shared<SessionMgr>();
	g_IOCPServer = std::make_shared<IOCPServer>();

	g_IOCPServer->StartIOCP();
	g_IOCPServer->AcceptClientLoop();
}

DWORD WINAPI ThreadComplete(LPVOID pParam)
{
	DWORD			dwTransferredSize = 0;
	DWORD			dwFlag = 0;
	Session* pSession = NULL;
	LPWSAOVERLAPPED	pWol = NULL;
	BOOL			bResult;
	puts("[IOCP �۾��� ������ ����]");
	while (1)
	{
		bResult = ::GetQueuedCompletionStatus(
			g_IOCPServer->GetIOCPHandle(),				//Dequeue�� IOCP �ڵ�.
			&dwTransferredSize,		//������ ������ ũ��.
			(PULONG_PTR)&pSession,	//���ŵ� �����Ͱ� ����� �޸�
			&pWol,					//OVERLAPPED ����ü.
			INFINITE);				//�̺�Ʈ�� ������ ���.

		shared_ptr<Session> SessionPtr = g_SessionMgr->FindSession(pSession);
		OVERLAPPED_EX* pOverEx = (OVERLAPPED_EX*)pWol;

		if (SessionPtr && bResult == TRUE)
		{
			//�������� ���.
			
			/////////////////////////////////////////////////////////////
			//1. Ŭ���̾�Ʈ�� ������ ���������� �ݰ� ������ ���� ���.
			if (dwTransferredSize == 0)
			{

				g_SessionMgr->RemoveSession(SessionPtr);
				//CloseClient(pSession->hSocket);
				//delete pWol;
				//delete pSession;
				puts("\tGQCS: Ŭ���̾�Ʈ�� ���������� ������ ������.");
			}

			/////////////////////////////////////////////////////////////
			//2. Ŭ���̾�Ʈ�� ���� �����͸� ������ ���.
			else
			{
				if (pOverEx->operationType == 1)
				{
					SessionPtr->SetReadyToSend(dwTransferredSize);
					g_SessionMgr->SendAllMessage(SessionPtr);

					//�ٽ� IOCP�� ���.
					SessionPtr->OnRecive();
				}
			}
		}
		else
		{
			//���������� ���.

			/////////////////////////////////////////////////////////////
			//3. �Ϸ� ť���� �Ϸ� ��Ŷ�� ������ ���ϰ� ��ȯ�� ���.
			if (pWol == NULL)
			{
				//IOCP �ڵ��� ���� ���(������ �����ϴ� ���)�� �ش�ȴ�.
				puts("\tGQCS: IOCP �ڵ��� �������ϴ�.");
				break;
			}

			/////////////////////////////////////////////////////////////
			//4. Ŭ���̾�Ʈ�� ������������ ����ưų�
			//   ������ ���� ������ ������ ���.
			else
			{
				if (pSession != NULL)
				{
					g_SessionMgr->RemoveSession(SessionPtr);
					//delete pWol;
					//delete pSession;
				}

				puts("\tGQCS: ���� ���� Ȥ�� �������� ���� ����");
			}
		}
	}

	puts("[IOCP �۾��� ������ ����]");
	return 0;
}

SessionMgr::SessionMgr()
{
}
void SessionMgr::AddClientSession(shared_ptr<Session> pSession)
{
	auto tempPtr = pSession.get();
	{
		std::unique_lock<std::shared_mutex> wLock{ m_rwLock };
		auto iter = m_listSession.emplace(m_listSession.end(), std::move(pSession));
		m_mapSessionIter.emplace(tempPtr, std::move(iter));
	}
}
void SessionMgr::RemoveSession(shared_ptr<Session> pSession)
{
	{
		std::unique_lock<std::shared_mutex> wLock{ m_rwLock };
		m_listSession.erase(m_mapSessionIter.extract(pSession.get()).mapped());
	}
}
shared_ptr<Session> SessionMgr::FindSession(Session* pSessionKey)
{
	std::shared_lock<std::shared_mutex> wLock{ m_rwLock };
	return *m_mapSessionIter.find(pSessionKey)->second;
}

void SessionMgr::SendAllMessage(shared_ptr<Session> pSession)
{
	{
		std::shared_lock<std::shared_mutex> wLock{ m_rwLock };
		for (const auto& session : m_listSession)
		{
			pSession->OnSend(session);
		}
	}
}