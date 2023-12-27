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


template<typename T = char>
class Buffer
{
private:
	std::unique_ptr<T[]> data;
	T* data_ptr;
	int len;
public:
	Buffer(const int n)noexcept
		:data{ std::make_unique_for_overwrite<T[]>(n) }
		, data_ptr{ data.get() }
		, len{ n }
	{}
	T& operator[](const int idx)noexcept { return data_ptr[idx]; }
	operator T* () noexcept { return data_ptr; }
	T* begin() noexcept { return data_ptr; }
	T* end() noexcept { return data_ptr + len; }
	const T* begin() const noexcept { return data_ptr; }
	const T* end() const noexcept { return data_ptr + len; }
	int Size()const noexcept { return len; }
};

void HandleError(string_view strCause)
{
	cout << strCause << " Error Code: " << ::WSAGetLastError() << endl;
}

template<typename SendItem>
void SendAuto(SOCKET hClient, SendItem& item)noexcept {
	::send(hClient, (char*)&item, sizeof(item), 0);
}

void GetFileList(SOCKET hSocket)
{
	MYCMD cmd{ CMDCODE::CMD_GET_LIST,0 };
	// �������� ���ϸ���Ʈ �޶���
	SendAuto(hSocket, cmd);

	// ����Ʈ �޴°� �������˸� (�ڿ��� �޾ƾ��� ������ ��������)
	::recv(hSocket, (char*)&cmd, sizeof(cmd), 0);
	if (CMDCODE::CMD_SND_FILELIST != cmd.nCode)HandleError("����Ʈ������");

	SEND_FILELIST fileList;
	::recv(hSocket, (char*)&fileList, sizeof(fileList), 0);

	FILEINFO fInfo;
	// ���ϸ���Ʈ������ ���ʷ� ������ ���
	for (int i = 0; i < fileList.nCount; ++i)
	{
		::recv(hSocket, (char*)&fInfo, sizeof(fInfo), 0);
		printf("%d\t%s\t%d\n",
			fInfo.nIndex, fInfo.szFileName, fInfo.dwFileSize);
	}
}

void GetFile(SOCKET hSocket)
{
	// �������� �Է�
	int nIndex;
	printf("������ ������ �ε���(0~2)�� �Է��ϼ���.: ");
	fflush(stdin);
	scanf_s("%d", &nIndex);

	// ������ �������� ��û
	Buffer pCommand{ sizeof(MYCMD) + sizeof(GETFILE) };

	MYCMD* pCmd = (MYCMD*)pCommand.begin();
	pCmd->nCode = CMDCODE::CMD_GET_FILE;
	pCmd->nSize = sizeof(GETFILE);

	GETFILE* pFile = (GETFILE*)(pCommand + sizeof(MYCMD)); // �̸�ŭ �����ϸ� GETFILE�� ����� ���ۉ�
	pFile->nIndex = nIndex;

	
	// ��� + �����ε����� �ѹ��� �۽�
	::send(hSocket, pCommand, pCommand.Size(), 0);

	// ������� �ϴ� �������� �� ������ + �����ε��� ������
	// �� ������ ������ ������ �̸��� ũ�⸦ �̸� �޴´�
	MYCMD cmd = {};
	FILEINFO fInfo = {};
	::recv(hSocket, (char*)&cmd, sizeof(cmd), 0);
	if (CMDCODE::CMD_ERROR == cmd.nCode)
	{
		ERRORDATA err = {};
		::recv(hSocket, (char*)&err, sizeof(err), 0);
		HandleError("��������������");
	}
	else
		::recv(hSocket, (char*)&fInfo, sizeof(fInfo), 0);

	// ������� ���� �̸��� ũ�⸦ �޾����� �װ����� ���ϸ���� �����غ�
	Buffer fileBuff(65536);
	FILE* fp = NULL;
	errno_t nResult = fopen_s(&fp, fInfo.szFileName, "wb");
	if (nResult != 0)
		HandleError("������ ���� �� �� �����ϴ�.");
	DWORD dwTotalRecv = 0; // ���ݱ��� ���������ѷ�
	int nRecv;
	while ((nRecv = ::recv(hSocket, fileBuff, 65536, 0)) > 0)
	{
		fwrite(fileBuff, nRecv, 1, fp);
		dwTotalRecv += nRecv;
		putchar('#');

		if (dwTotalRecv >= fInfo.dwFileSize)
		{
			putchar('\n');
			puts("���� ���ſϷ�!");
			break;
		}
	}
	fclose(fp);
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
	// ������ ���ϸ���Ʈ�ּ���
	GetFileList(hSocket);
	// ���۹������ϼ��� / �ޱ�
	GetFile(hSocket);


	::closesocket(hSocket);
	::WSACleanup();
}