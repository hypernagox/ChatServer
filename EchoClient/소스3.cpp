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
//MYCMD 구조체의 nCode 멤버에 적용될 수 있는 값
enum class CMDCODE
{
	CMD_ERROR = 50,			//에러
	CMD_GET_LIST = 100,			//C->S: 파일 리스트 요구
	CMD_GET_FILE,				//C->S: 파일 전송 요구
	CMD_SND_FILELIST = 200,		//S->C: 파일 리스트 전송
	CMD_BEGIN_FILE			//S->C: 파일 전송 시작을 알림.
};

/////////////////////////////////////////////////////////////////////////
//기본헤더
typedef struct MYCMD
{
	CMDCODE nCode;			//명령코드
	int nSize;			//데이터의 바이트 단위 크기
} MYCMD;

/////////////////////////////////////////////////////////////////////////
//확장헤더: 에러 메시지 전송헤더
typedef struct ERRORDATA
{
	int	nErrorCode;		//에러코드: ※향후 확장을 위한 멤버다.
	char szDesc[256];	//에러내용
} ERRORDATA;

/////////////////////////////////////////////////////////////////////////
//확장헤더: S->C: 파일 리스트 전송
typedef struct SEND_FILELIST
{
	unsigned int nCount;	//전송할 파일정보(GETFILE 구조체) 개수
} SEND_FILELIST;

/////////////////////////////////////////////////////////////////////////
//확장헤더: CMD_GET_FILE
typedef struct GETFILE
{
	unsigned int nIndex;	//전송받으려는 파일의 인덱스
} GETFILE;

/////////////////////////////////////////////////////////////////////////
//확장헤더: 
typedef struct FILEINFO
{
	unsigned int nIndex;			//파일의 인덱스
	char szFileName[_MAX_FNAME];	//파일이름
	DWORD dwFileSize;				//파일의 바이트 단위 크기
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
	// 서버에게 파일리스트 달라함
	SendAuto(hSocket, cmd);

	// 리스트 받는거 시작을알림 (뒤에는 받아야할 갯수가 적혀있음)
	::recv(hSocket, (char*)&cmd, sizeof(cmd), 0);
	if (CMDCODE::CMD_SND_FILELIST != cmd.nCode)HandleError("리스트못받음");

	SEND_FILELIST fileList;
	::recv(hSocket, (char*)&fileList, sizeof(fileList), 0);

	FILEINFO fInfo;
	// 파일리스트정보를 차례로 받으며 출력
	for (int i = 0; i < fileList.nCount; ++i)
	{
		::recv(hSocket, (char*)&fInfo, sizeof(fInfo), 0);
		printf("%d\t%s\t%d\n",
			fInfo.nIndex, fInfo.szFileName, fInfo.dwFileSize);
	}
}

void GetFile(SOCKET hSocket)
{
	// 뭐받을지 입력
	int nIndex;
	printf("수신할 파일의 인덱스(0~2)를 입력하세요.: ");
	fflush(stdin);
	scanf_s("%d", &nIndex);

	// 서버에 파일전송 요청
	Buffer pCommand{ sizeof(MYCMD) + sizeof(GETFILE) };

	MYCMD* pCmd = (MYCMD*)pCommand.begin();
	pCmd->nCode = CMDCODE::CMD_GET_FILE;
	pCmd->nSize = sizeof(GETFILE);

	GETFILE* pFile = (GETFILE*)(pCommand + sizeof(MYCMD)); // 이만큼 전진하면 GETFILE의 블록이 시작됌
	pFile->nIndex = nIndex;

	
	// 명령 + 파일인덱스를 한번에 송신
	::send(hSocket, pCommand, pCommand.Size(), 0);

	// 여기까지 일단 서버한테 나 파일줘 + 파일인덱스 보냈음
	// 찐 파일이 오기전 파일의 이름과 크기를 미리 받는다
	MYCMD cmd = {};
	FILEINFO fInfo = {};
	::recv(hSocket, (char*)&cmd, sizeof(cmd), 0);
	if (CMDCODE::CMD_ERROR == cmd.nCode)
	{
		ERRORDATA err = {};
		::recv(hSocket, (char*)&err, sizeof(err), 0);
		HandleError("파일정보못받음");
	}
	else
		::recv(hSocket, (char*)&fInfo, sizeof(fInfo), 0);

	// 여기까지 파일 이름과 크기를 받았으니 그걸토대로 파일만들고 받을준비
	Buffer fileBuff(65536);
	FILE* fp = NULL;
	errno_t nResult = fopen_s(&fp, fInfo.szFileName, "wb");
	if (nResult != 0)
		HandleError("파일을 생성 할 수 없습니다.");
	DWORD dwTotalRecv = 0; // 지금까지 받은파일총량
	int nRecv;
	while ((nRecv = ::recv(hSocket, fileBuff, 65536, 0)) > 0)
	{
		fwrite(fileBuff, nRecv, 1, fp);
		dwTotalRecv += nRecv;
		putchar('#');

		if (dwTotalRecv >= fInfo.dwFileSize)
		{
			putchar('\n');
			puts("파일 수신완료!");
			break;
		}
	}
	fclose(fp);
}

int main()
{
	// 윈속초기화
	WSADATA wsa = {};
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("윈속초기화실패");
	}

	// 소켓만들기(클라)
	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == hSocket)
	{
		HandleError("소켓만들기실패");
	}

	// 포트커넥트 (서버쪽소켓과 연결한다)
	SOCKADDR_IN server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(25000);
	server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.0.43");
	if (
		::connect(hSocket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR
		)
	{
		HandleError("연결실패");
	}
	// 서버님 파일리스트주세요
	GetFileList(hSocket);
	// 전송받을파일선택 / 받기
	GetFile(hSocket);


	::closesocket(hSocket);
	::WSACleanup();
}