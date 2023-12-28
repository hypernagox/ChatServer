#include <iostream>
#include <Windows.h>
#include <memory>
#include <tchar.h>
#include <unordered_map>
using namespace std;

struct HANDLE_AND_CALLBACK
{
	HANDLE hHandle = NULL;
	LPOVERLAPPED_COMPLETION_ROUTINE fpCallBack = NULL;
};

void CALLBACK FileIoComplete(
	DWORD dwError,		//에러코드
	DWORD dwTransfered,	//입/출력이 완료된 데이터 크기
	LPOVERLAPPED pOl)	//OVERLAPPED 구조체
{
	printf("FileIoComplete() Callback - [%d 바이트] 쓰기완료 - %s\n",
		dwTransfered, (char*)pOl->hEvent);

	//hEvent 멤버를 포인터로 전용했으므로 가리키는 대상 메모리 해제한다.
	//이 메모리는 IoThreadFunction() 함수에서 동적 할당한 것들이다!
	delete[] pOl->hEvent;
	delete pOl;
	puts("FileIoComplete() - return\n");
}


/////////////////////////////////////////////////////////////////////////
DWORD WINAPI IoThreadFunction(LPVOID pParam)
{
	HANDLE_AND_CALLBACK* handle_callback = (HANDLE_AND_CALLBACK*)pParam;
	char* pSzBuffer = new char[16];
	::memset(pSzBuffer, 0, sizeof(char) * 16);
	::strcpy_s(pSzBuffer, sizeof(char) * 16, "Hello IOCP");
	//::memcpy(pSzBuffer, "Hello IOCP", sizeof(char) * 16);
	LPOVERLAPPED pOverlapped = new OVERLAPPED;
	::memset(pOverlapped, 0, sizeof(OVERLAPPED));

	pOverlapped->Offset = 1024 * 1024 * 512;	//512MB
	pOverlapped->hEvent = pSzBuffer;

	puts("IoThreadFunction() - 중첩된 쓰기 시도");
	::WriteFileEx(handle_callback->hHandle,
		pSzBuffer,
		sizeof(char) * 16,
		pOverlapped,
		handle_callback->fpCallBack);

	//비동기 쓰기 시도에 대해 Alertable wait 상태로 대기한다.
	for (; ::SleepEx(1, TRUE) != WAIT_IO_COMPLETION; );
	puts("IoThreadFunction() - return");
	return 0;
}

int main()
{
	unordered_map<int, LPOVERLAPPED_COMPLETION_ROUTINE> umap
	{
		{0,FileIoComplete}
	};
	// 파일을 생성하는데 중첩속성을 부여함.
	HANDLE hFile = ::CreateFile(
		_T("TestFile.txt"),
		GENERIC_WRITE,
		0, // 공유안함 ( 여러스레드 동시접근 X)
		NULL, // 보안레벨 상속값 사용
		CREATE_ALWAYS, // 무조건 생성
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // 중첩된쓰기.
		NULL
	);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		cout << "파일열기실패" << endl;
	}

	HANDLE_AND_CALLBACK stFac{ hFile,umap[0]};

	//비동기 쓰기를 위한 스레드를 생성한다.
	HANDLE hThread = NULL;
	DWORD dwThreadID = 0;

	dwThreadID = 0;
	hThread = ::CreateThread(
		NULL,	//보안속성 상속.
		0,		//스택 메모리는 기본크기(1MB).
		IoThreadFunction,	//스래드로 실행할 함수이름.
		&stFac,	//함수에 전달할 매개변수.
		0,		//생성 플래그는 기본값 사용.
		&dwThreadID);	//생성된 스레드ID 저장.

	//작업자 스레드가 종료될 때까지 기다린다.
	::WaitForSingleObject(hThread, INFINITE);

	//파일을 닫고 종료한다.
	puts("main() thread 종료");
	::CloseHandle(hFile);
	return 0;
}