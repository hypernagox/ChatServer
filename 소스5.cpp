#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <string_view>
#include <vector>
#pragma comment(lib, "ws2_32")
#include <tchar.h>
#include <Mswsock.h>
#pragma comment(lib, "Mswsock")
#include <unordered_map>
#include <variant>
#include <functional>
#include <memory>
using namespace std;

//using CallBackFunc = std::variant<std::monostate
//	, std::function<void(void)>
//	, std::function<int(int,int)>
//	, std::function<string(string)>
//>;
//
//unordered_map<int, CallBackFunc> g_LookUpTable;
//
//template<typename ReturnType = void,typename Key, typename... Args>
//constexpr ReturnType CallLookUpTable(Key&& key, Args&&... args) noexcept
//{
//	return std::visit([...args = std::forward<Args>(args)](auto&& callBack) mutable noexcept ->ReturnType{
//		if constexpr (std::is_invocable_v<decltype(callBack), Args...>)return callBack(std::forward<Args>(args)...);
//		std::cout << "Function Arg or Type MissMatch !" << endl;
//		if constexpr (! std::is_same_v<std::invoke_result<decltype(callBack), Args...>, void>)return ReturnType{};
//		}, g_LookUpTable.find(std::forward<Key>(key))->second);
//}
//
//template<typename Key,typename Func>
//void RegisterLookUpTable(Key&& key, Func&& func)noexcept { g_LookUpTable.emplace(std::forward<Key>(key), std::forward<Func>(func)); }

template<typename Enum>
constexpr int etoi(const Enum eType)noexcept { return static_cast<int>(eType); }
/////////////////////////////////////////////////////////////////////////
//MYCMD ����ü�� nCode ����� ����� �� �ִ� ��
enum class CMDCODE 
{
	CMD_ERROR = 50,			//����
	CMD_GET_LIST = 100,			//C->S: ���� ����Ʈ �䱸
	CMD_GET_FILE,				//C->S: ���� ���� �䱸
	CMD_SND_FILELIST = 200,		//S->C: ���� ����Ʈ ����
	CMD_BEGIN_FILE			//S->C: ���� ���� ������ �˸�.
};

/////////////////////////////////////////////////////////////////////////
//�⺻���
typedef struct MYCMD
{
	CMDCODE nCode;			//����ڵ�
	int nSize;			//�������� ����Ʈ ���� ũ��
} MYCMD;

/////////////////////////////////////////////////////////////////////////
//Ȯ�����: ���� �޽��� �������
typedef struct ERRORDATA
{
	int	nErrorCode;		//�����ڵ�: ������ Ȯ���� ���� �����.
	char szDesc[256];	//��������
} ERRORDATA;

/////////////////////////////////////////////////////////////////////////
//Ȯ�����: S->C: ���� ����Ʈ ����
typedef struct SEND_FILELIST
{
	unsigned int nCount;	//������ ��������(GETFILE ����ü) ����
} SEND_FILELIST;

/////////////////////////////////////////////////////////////////////////
//Ȯ�����: CMD_GET_FILE
typedef struct GETFILE
{
	unsigned int nIndex;	//���۹������� ������ �ε���
} GETFILE;

/////////////////////////////////////////////////////////////////////////
//Ȯ�����: 
typedef struct FILEINFO
{
	unsigned int nIndex;			//������ �ε���
	char szFileName[_MAX_FNAME];	//�����̸�
	DWORD dwFileSize;				//������ ����Ʈ ���� ũ��
} FILEINFO;

/// <summary>
/// ���� ����
/// </summary>

template<typename T = char>
class Buffer
{
private:
	std::unique_ptr<T[]> data;
	T* data_ptr;
	int len;
public:
	Buffer(const int n)noexcept
		:data{std::make_unique_for_overwrite<T[]>(n)}
		,data_ptr{data.get()}
		,len{n}
	{}
	T& operator[](const int idx)noexcept { return data_ptr[idx]; }
	operator T* () noexcept{ return data_ptr; }
	T* begin() noexcept{ return data_ptr; }
	T* end() noexcept{ return data_ptr + len; }
	const T* begin() const noexcept { return data_ptr; }
	const T* end() const noexcept { return data_ptr + len; }
};

using CallBackFunc = std::function<void(SOCKET)>;

SEND_FILELIST g_flist = { 3 };

//Ŭ���̾�Ʈ�� �ٿ�ε� ������ ����
FILEINFO g_aFInfo[3] = {
	{ 0, "Sleep Away.mp3", 4842585 },
	{ 1, "Kalimba.mp3", 8414449 },
	{ 2, "Maid with the Flaxen Hair.mp3", 4113874 }
};

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

template<typename SendItem>
void SendAuto(SOCKET hClient, SendItem& item)noexcept {
	::send(hClient, (char*)&item, sizeof(item), 0);
}
void SendFileList(SOCKET hClient)
{
	MYCMD cmd;
	cmd.nCode = CMDCODE::CMD_SND_FILELIST;
	cmd.nSize = sizeof(g_flist) + sizeof(g_aFInfo);
	// ����ڵ�����
	::send(hClient, (char*)&cmd, sizeof(cmd), 0);
	
	// ���ϸ���Ʈ ����
	::send(hClient, (char*)&g_flist, sizeof(g_flist), 0);
	// ���� ������ ����
	::send(hClient, (char*)g_aFInfo, sizeof(g_aFInfo), 0);
}

void SendFile(SOCKET hClient)
{
	puts("Ŭ���̾�Ʈ�� ���������� �䱸��.");
	// ���� �䱸�� �ε����� �޴´�.
	GETFILE file;
	::recv(hClient, (char*)&file, sizeof(file), 0);

	MYCMD cmd;
	ERRORDATA err;
	const int nIdx = file.nIndex;
	cout << nIdx << endl;
	// �ε��� ���� �� �����ڵ� ����
	if (nIdx < 0 || nIdx >2)
	{
		cmd.nCode = CMDCODE::CMD_ERROR;
		cmd.nSize = sizeof(err);
		err.nErrorCode = 0;
		strcpy_s(err.szDesc, "�����ε�������");
		// ���� ����ڵ���� ����
		SendAuto(hClient, cmd);
		// �����ڵ带 ����
		SendAuto(hClient, err);
		return;
	}
	// ���ϼ۽��� ������ �˸���, �ڿ� �� �� ����ü ����� �̸� �˷��ش�
	cmd.nCode = CMDCODE::CMD_BEGIN_FILE;
	cmd.nSize = sizeof(FILEINFO);
	SendAuto(hClient, cmd);
	// �ڿ� �ð� �˷������� �ϰ� �����ֱ���Ѱ� �����ش� (���� �̸�,����ũ��)
	SendAuto(hClient, g_aFInfo[nIdx]);
	cout << "aa";
	// Ŭ������ �����̸��� ũ�⸦�˷������� ���� ���ϸ��� �����غ��Ѵ�.
	FILE* fp = NULL;
	errno_t nResult = fopen_s(&fp, g_aFInfo[nIdx].szFileName, "rb");
	if (nResult != 0)
		HandleError("������ ������ ������ �� �����ϴ�.");

	//������ �۽��Ѵ�.
	Buffer byBuffer(65536);
	int nRead;
	while ((nRead = fread(byBuffer, sizeof(char), 65536, fp)) > 0)
		send(hClient, byBuffer, nRead, 0);
}
unordered_map<CMDCODE, CallBackFunc> g_LookUpTable
{
	{CMDCODE::CMD_GET_LIST,SendFileList},
	{CMDCODE::CMD_GET_FILE,SendFile}
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

	cout << "Ŭ�� ��ٸ�������" << endl;
	// 3. ���Ӵ�� ���·�
	if (::listen(hSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		HandleError("�����������Խ���");
	}

	// 4. Ŭ���̾�Ʈ ����ó�� �� ���� (Ŭ���� ����)
	// 4.1 Ŭ���̾�Ʈ ������ ������ ����(����)

	SOCKADDR_IN clientaddr = { 0 };
	int nAddrLen = sizeof(clientaddr);
	SOCKET hClient = ::accept(hSocket, (SOCKADDR*)&clientaddr, &nAddrLen);
	if (INVALID_SOCKET == hClient)
	{
		HandleError("Ŭ����޾���");
	}
	cout << "Ŭ�����ӿϷ�" << endl;

	// ����ڵ� ����ü
	MYCMD cmd;

	while (
		::recv(hClient, (char*)&cmd, sizeof(MYCMD),0) > 0
		)
	{
		g_LookUpTable[cmd.nCode](hClient);
	}

}