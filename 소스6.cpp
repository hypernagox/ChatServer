#include <iostream>
#include <Windows.h>
#include <tchar.h>
using namespace std;

int main()
{
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
	// �񵿱� ����� ���õ� ��� Ư�� �������� ����ü�� �߿�
	DWORD dwRead;
	OVERLAPPED aOl[3] = {};
	HANDLE aEvt[3] = { 0 }; // �񵿱��۾��� ����� ����� �ڵ���
	int cnt = 0;
	for (auto& ol : aOl)
	{
		aEvt[cnt++] = ol.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		
	}

	// 3���� �񵿱⾲�⸦ �Ұǵ� �� ������ �������� ����.
	aOl[0].Offset = 0;
	aOl[1].Offset = 1024 * 1024 * 128;
	aOl[2].Offset = 16;

	// 3���� �����۾��� �ü���� �˾Ƽ� ��Ƽ������� �񵿱�� ���� �۾�
	// �׷��� 1��ó�� �������� ���ϰ�ũ�� �װ� Ȯ���ϴ°� ��ü�� �ð��� �ɸ��� ���� ���� ���߿� �Ϸ�� ���ɼ��� ũ��
	for (int i = 0; i < 3; ++i)
	{
		printf("%d��° ��ø�� ���� �õ�.\n", i);
		::WriteFile(hFile, "0123456789", 10, &dwRead, &aOl[i]);
		//�������� ��� ���� �õ��� ����(����)�ȴ�!
		// ���? OS���� IO��û �����Ϸ�
		if (::GetLastError() != ERROR_IO_PENDING)
			exit(0);
	}
	// �񵿱Ⱑ �����°� ��ٸ�
	DWORD dwResult;
	for (int i = 0; i < 3; ++i)
	{
		dwResult = ::WaitForMultipleObjects(3, aEvt, FALSE, INFINITE);
		printf("-> %d��° ���� �Ϸ�.\n", dwResult - WAIT_OBJECT_0);
	}

	//�̺�Ʈ �ڵ�� ������ �ݴ´�.
	for (int i = 0; i < 3; ++i)
		::CloseHandle(aEvt[i]);
	::CloseHandle(hFile);
}