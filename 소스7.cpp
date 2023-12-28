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
	DWORD dwError,		//�����ڵ�
	DWORD dwTransfered,	//��/����� �Ϸ�� ������ ũ��
	LPOVERLAPPED pOl)	//OVERLAPPED ����ü
{
	printf("FileIoComplete() Callback - [%d ����Ʈ] ����Ϸ� - %s\n",
		dwTransfered, (char*)pOl->hEvent);

	//hEvent ����� �����ͷ� ���������Ƿ� ����Ű�� ��� �޸� �����Ѵ�.
	//�� �޸𸮴� IoThreadFunction() �Լ����� ���� �Ҵ��� �͵��̴�!
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

	puts("IoThreadFunction() - ��ø�� ���� �õ�");
	::WriteFileEx(handle_callback->hHandle,
		pSzBuffer,
		sizeof(char) * 16,
		pOverlapped,
		handle_callback->fpCallBack);

	//�񵿱� ���� �õ��� ���� Alertable wait ���·� ����Ѵ�.
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
	// ������ �����ϴµ� ��ø�Ӽ��� �ο���.
	HANDLE hFile = ::CreateFile(
		_T("TestFile.txt"),
		GENERIC_WRITE,
		0, // �������� ( ���������� �������� X)
		NULL, // ���ȷ��� ��Ӱ� ���
		CREATE_ALWAYS, // ������ ����
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // ��ø�Ⱦ���.
		NULL
	);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		cout << "���Ͽ������" << endl;
	}

	HANDLE_AND_CALLBACK stFac{ hFile,umap[0]};

	//�񵿱� ���⸦ ���� �����带 �����Ѵ�.
	HANDLE hThread = NULL;
	DWORD dwThreadID = 0;

	dwThreadID = 0;
	hThread = ::CreateThread(
		NULL,	//���ȼӼ� ���.
		0,		//���� �޸𸮴� �⺻ũ��(1MB).
		IoThreadFunction,	//������� ������ �Լ��̸�.
		&stFac,	//�Լ��� ������ �Ű�����.
		0,		//���� �÷��״� �⺻�� ���.
		&dwThreadID);	//������ ������ID ����.

	//�۾��� �����尡 ����� ������ ��ٸ���.
	::WaitForSingleObject(hThread, INFINITE);

	//������ �ݰ� �����Ѵ�.
	puts("main() thread ����");
	::CloseHandle(hFile);
	return 0;
}