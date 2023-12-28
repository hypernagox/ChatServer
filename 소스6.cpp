#include <iostream>
#include <Windows.h>
#include <tchar.h>
using namespace std;

int main()
{
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
	// 비동기 쓰기와 관련된 놈들 특히 오버랩드 구조체가 중요
	DWORD dwRead;
	OVERLAPPED aOl[3] = {};
	HANDLE aEvt[3] = { 0 }; // 비동기작업의 결과를 통신할 핸들임
	int cnt = 0;
	for (auto& ol : aOl)
	{
		aEvt[cnt++] = ol.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		
	}

	// 3연속 비동기쓰기를 할건데 각 쓰기의 오프셋을 지정.
	aOl[0].Offset = 0;
	aOl[1].Offset = 1024 * 1024 * 128;
	aOl[2].Offset = 16;

	// 3개의 쓰기작업이 운영체제가 알아서 멀티스레드로 비동기로 동시 작업
	// 그래서 1번처럼 오프셋이 과하게크면 그거 확보하는거 자체가 시간이 걸리고 보통 가장 나중에 완료될 가능성이 크다
	for (int i = 0; i < 3; ++i)
	{
		printf("%d번째 중첩된 쓰기 시도.\n", i);
		::WriteFile(hFile, "0123456789", 10, &dwRead, &aOl[i]);
		//정상적인 경우 쓰기 시도는 지연(보류)된다!
		// 펜딩? OS에게 IO요청 접수완료
		if (::GetLastError() != ERROR_IO_PENDING)
			exit(0);
	}
	// 비동기가 끝나는걸 기다림
	DWORD dwResult;
	for (int i = 0; i < 3; ++i)
	{
		dwResult = ::WaitForMultipleObjects(3, aEvt, FALSE, INFINITE);
		printf("-> %d번째 쓰기 완료.\n", dwResult - WAIT_OBJECT_0);
	}

	//이벤트 핸들과 파일을 닫는다.
	for (int i = 0; i < 3; ++i)
		::CloseHandle(aEvt[i]);
	::CloseHandle(hFile);
}