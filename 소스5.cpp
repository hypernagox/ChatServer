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

/// <summary>
/// 이제 시작
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

//클라이언트가 다운로드 가능한 파일
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
	// 명령코드전송
	::send(hClient, (char*)&cmd, sizeof(cmd), 0);
	
	// 파일리스트 전송
	::send(hClient, (char*)&g_flist, sizeof(g_flist), 0);
	// 파일 정보들 전송
	::send(hClient, (char*)g_aFInfo, sizeof(g_aFInfo), 0);
}

void SendFile(SOCKET hClient)
{
	puts("클라이언트가 파일전송을 요구함.");
	// 파일 요구한 인덱스를 받는다.
	GETFILE file;
	::recv(hClient, (char*)&file, sizeof(file), 0);

	MYCMD cmd;
	ERRORDATA err;
	const int nIdx = file.nIndex;
	cout << nIdx << endl;
	// 인덱스 오류 시 오류코드 전송
	if (nIdx < 0 || nIdx >2)
	{
		cmd.nCode = CMDCODE::CMD_ERROR;
		cmd.nSize = sizeof(err);
		err.nErrorCode = 0;
		strcpy_s(err.szDesc, "파일인덱스오류");
		// 오류 명령코드먼저 전송
		SendAuto(hClient, cmd);
		// 에러코드를 전송
		SendAuto(hClient, err);
		return;
	}
	// 파일송신의 시작을 알리며, 뒤에 더 올 구조체 사이즈를 미리 알려준다
	cmd.nCode = CMDCODE::CMD_BEGIN_FILE;
	cmd.nSize = sizeof(FILEINFO);
	SendAuto(hClient, cmd);
	// 뒤에 올거 알려줬으니 믿고 보내주기로한거 보내준다 (파일 이름,파일크기)
	SendAuto(hClient, g_aFInfo[nIdx]);
	cout << "aa";
	// 클라한테 파일이름과 크기를알려줬으니 이제 파일만들어서 보낼준비를한다.
	FILE* fp = NULL;
	errno_t nResult = fopen_s(&fp, g_aFInfo[nIdx].szFileName, "rb");
	if (nResult != 0)
		HandleError("전송할 파일을 개방할 수 없습니다.");

	//파일을 송신한다.
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
	// 윈속 초기화
	WSADATA wsa = { 0 };
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		HandleError("윈속 초기화 실패");
	}

	// 1. 접속 대기 소켓을 생성한다
	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
	{
		HandleError("소켓생성실패");
	}

	// 2. 포트 바인딩
	SOCKADDR_IN svraddr = { 0 };
	svraddr.sin_family = AF_INET;
	svraddr.sin_port = htons(25000);
	svraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	if (::bind(
		hSocket, (SOCKADDR*)&svraddr, sizeof(svraddr)) == SOCKET_ERROR)
	{
		HandleError("바인딩 실패");
	}

	cout << "클라를 기다리고있음" << endl;
	// 3. 접속대기 상태로
	if (::listen(hSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		HandleError("리슨상태진입실패");
	}

	// 4. 클라이언트 접속처리 및 대응 (클라의 소켓)
	// 4.1 클라이언트 연결후 새소켓 생성(개방)

	SOCKADDR_IN clientaddr = { 0 };
	int nAddrLen = sizeof(clientaddr);
	SOCKET hClient = ::accept(hSocket, (SOCKADDR*)&clientaddr, &nAddrLen);
	if (INVALID_SOCKET == hClient)
	{
		HandleError("클라못받았음");
	}
	cout << "클라접속완료" << endl;

	// 명령코드 구조체
	MYCMD cmd;

	while (
		::recv(hClient, (char*)&cmd, sizeof(MYCMD),0) > 0
		)
	{
		g_LookUpTable[cmd.nCode](hClient);
	}

}