/*
Name: win32.cpp
Author: Nathan LeRoux
Purpose: Windows specific code
*/

#include "..\include.h"
#include "..\game\game.h"

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>

void platformDebugOut(const char * format)
{
	printf("%s", format);
	OutputDebugStringA(format);
}

void platformFatal(const char * error)
{
#ifdef _DEBUG
	if(IsDebuggerPresent())
	{
		DebugBreak();
		exit(1);
	}
#endif

	MessageBox(NULL, error, "nyEngine", MB_OK);
	exit(1);
}

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ExceptionInfo)
{
	typedef BOOL (*PDUMPFN)(
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);

	HANDLE hFile = CreateFile("minidump.dmp", GENERIC_READ | GENERIC_WRITE, 0,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	HMODULE h = LoadLibrary("DbgHelp.dll");
	PDUMPFN pFn = (PDUMPFN)GetProcAddress(h, "MiniDumpWriteDump");

	if(hFile != INVALID_HANDLE_VALUE)
	{
		if(pFn)
		{
			MINIDUMP_EXCEPTION_INFORMATION mdei;

			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers = ExceptionInfo;
			mdei.ClientPointers = FALSE;

			pFn(GetCurrentProcess(), GetCurrentProcessId(),
				hFile, MiniDumpNormal, ExceptionInfo ? &mdei : NULL, NULL, NULL);
		}

		CloseHandle(hFile);
	}

	printf("there once was a man from bungie; he tried to rhyme with bungie and then he exploded");

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	// There isn't much windows specific initialization to do here, just allocate a console in debug builds
	//     as well as reporting memory leaks
#ifdef _DEBUG
	AllocConsole();
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Setup our error handler
	SetUnhandledExceptionFilter(ExceptionHandler);

	// Run the game
	gameRun();

	return 0;
}
#endif
