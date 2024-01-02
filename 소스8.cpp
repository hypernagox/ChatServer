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
#include <unordered_set>
#include <functional>
#include "ConcurrentVector.hpp"

using std::list;
using std::vector;
using std::shared_ptr;
using std::unordered_map;
using std::unordered_set;

static constexpr int MAX_CLIENTS = 5000;

class IOCPServer;
class Session;

class SessionMgr
{
private:
	ConcurrentVector<Session*> m_vecSession;
	vector <std::pair<int, shared_ptr<Session>>> m_vecGarbage;
	unordered_map<DWORD, shared_ptr<Session>> m_mapSessionIter;
	unordered_set<DWORD> m_setEmptyIndex;
	std::mutex m_setLock;
	std::mutex m_mapLock;
	std::shared_mutex m_garbageLock;
	std::atomic_uint m_loopThreadCounter = 0;
	std::function<bool(Session*, Session*)> m_lookUp[2];
	Session* const m_dummyPtr;
public:
	SessionMgr();
	~SessionMgr();

	void AddClientSession(shared_ptr<Session> pSession, int& ID, HANDLE iocpHandle);
	
	void RemoveSession(shared_ptr<Session> pSession);
	
	shared_ptr<Session> FindSession(const int clientID);
	

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

	void SetOverLapped(const int id)
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
	bool m_bIsValid = true;
public:
	Session()
	{
	}

	~Session()
	{
		::shutdown(m_hClientSocket, SD_BOTH);
		::closesocket(m_hClientSocket);
	}

	void RegisterIOCP(HANDLE iocpHandle)
	{
		::CreateIoCompletionPort((HANDLE)m_hClientSocket, iocpHandle,
			(ULONG_PTR)m_iClientID,
			0);

		const std::wstring temp = std::format(_T("ID{} ����\n"), m_iClientID);
		m_stOverlappedSend.ReadyToSend((int)temp.size() * sizeof(wchar_t), (char*)temp.data());
		g_SessionMgr->SendAllMessage(this->shared_from_this());
	}

	bool OnAcceptClient(SOCKET hServerSocket)
	{
		SOCKADDR sockAddr;
		int	nAddrSize = sizeof(SOCKADDR);

		m_hClientSocket = ::accept(hServerSocket,
			&sockAddr, &nAddrSize);

		return INVALID_SOCKET != m_hClientSocket;
	}

	bool OnRecive()
	{
		return m_stOverlappedRecv.OnRecv(m_hClientSocket);
	}

	bool OnSend(Session* const other)
	{
		return m_stOverlappedSend.OnSend(other->GetClinetSocket());
	}

	bool Nothing(Session* const other)
	{
		return false;
	}

	SOCKET GetClinetSocket() { return m_hClientSocket; }
	void SetReadyToSend(const int len) {
		m_stOverlappedSend.ReadyToSend(len + m_stOverlappedRecv.offset,m_stOverlappedRecv.buffer);
	}
	void SetSession(const int ID)
	{
		m_stOverlappedRecv.SetOverLapped(ID);
		m_stOverlappedSend.SetOverLapped(ID);
		m_iClientID = ID;
	}
	const int GetClientID() { return m_iClientID; }
	void Disconnect() { m_bIsValid = false; }
	bool IsValid() { return m_bIsValid; }
};

DWORD WINAPI ThreadComplete(LPVOID pParam);

class IOCPServer
{
	static inline int g_clientID = 0;
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
			(pSession = std::make_shared<Session>())->OnAcceptClient(m_hServerSockket)
			)
		{
			auto tempPtr = pSession.get();
			g_SessionMgr->AddClientSession(std::move(pSession), g_clientID, m_IOCPHandle);
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
	int clientID = 0;
	LPWSAOVERLAPPED	pWol = NULL;
	BOOL			bResult;
	puts("[IOCP �۾��� ������ ����]");
	while (1)
	{
		bResult = ::GetQueuedCompletionStatus(
			g_IOCPServer->GetIOCPHandle(),				//Dequeue�� IOCP �ڵ�.
			&dwTransferredSize,		//������ ������ ũ��.
			(PULONG_PTR)&clientID,	//���ŵ� �����Ͱ� ����� �޸�
			&pWol,					//OVERLAPPED ����ü.
			INFINITE);				//�̺�Ʈ�� ������ ���.

		shared_ptr<Session> SessionPtr = g_SessionMgr->FindSession(clientID);
		OVERLAPPED_EX* pOverEx = (OVERLAPPED_EX*)pWol;

		if (SessionPtr && bResult == TRUE)
		{
			//�������� ���.
			if (!SessionPtr->IsValid())
			{
				continue;
			}
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
				if (SessionPtr != NULL)
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
	:m_dummyPtr{new Session}
{
	m_lookUp[0] = std::mem_fn(&Session::Nothing);
	m_lookUp[1]	= std::mem_fn(&Session::OnSend);
	m_vecSession.reserve(MAX_CLIENTS + 1);
	m_mapSessionIter.reserve(MAX_CLIENTS + 1);
	m_setEmptyIndex.reserve(MAX_CLIENTS + 1);
	m_dummyPtr->Disconnect();
}

SessionMgr::~SessionMgr()
{
	delete m_dummyPtr;
}

void SessionMgr::AddClientSession(shared_ptr<Session> pSession,int& ID,HANDLE iocpHandle)
{
	const auto tempPtr = pSession.get();
	int id = -1;
	bool bEmpty = false;
	{
		std::lock_guard<std::mutex>lock{ m_setLock };
		bEmpty = m_setEmptyIndex.empty();
		if (bEmpty)
		{
			id = ID++;
		}
		else
		{
			id = m_setEmptyIndex.extract(m_setEmptyIndex.begin()).value();
		}
	}
	{
		std::lock_guard<std::mutex> lock{ m_mapLock };
		m_mapSessionIter[id].swap(pSession);
	}
	tempPtr->SetSession(id);
	tempPtr->RegisterIOCP(iocpHandle);
	std::atomic_thread_fence(std::memory_order_seq_cst);
	if (bEmpty)
	{
		m_vecSession.emplace_back(tempPtr);
	}
	else
	{
		m_vecSession[id] = tempPtr;
	}
}
void SessionMgr::RemoveSession(shared_ptr<Session> pSession)
{
	pSession->Disconnect();
	const int id = pSession->GetClientID();
	m_vecSession[id] = m_dummyPtr;
	{
		std::shared_lock<std::shared_mutex> rLock{ m_garbageLock };
		m_vecGarbage.emplace_back(id, std::move(pSession));
	}
	{
		std::lock_guard<std::mutex> lock{ m_setLock };
		m_setEmptyIndex.emplace(id);
	}
}
shared_ptr<Session> SessionMgr::FindSession(const int clientID)
{
	std::lock_guard<std::mutex> lock{ m_mapLock };
	return m_mapSessionIter[clientID];
}

void SessionMgr::SendAllMessage(shared_ptr<Session> pSession)
{
	++m_loopThreadCounter;
	{
		std::shared_lock<std::shared_mutex> rLock{ m_garbageLock };
		for (const auto& session : m_vecSession)
		{
			const bool idx = pSession->IsValid() && session->IsValid();
			m_lookUp[idx](pSession.get(), session);
		}
	}
	if (0 == --m_loopThreadCounter)
	{
		std::scoped_lock lock{ m_garbageLock,m_mapLock };
		for (const auto& [idx, ptr] : m_vecGarbage)
		{
			m_mapSessionIter[idx] = nullptr;
		}
		m_vecGarbage.clear();
	}
}