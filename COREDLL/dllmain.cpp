// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

typedef UINT(__cdecl* CE_ENTRYPOINT)(HINSTANCE, HINSTANCE, LPCWSTR, int);

#undef __stdcall // DllMain should be __stdcall always
BOOL __stdcall DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		freopen("CONIN$", "r", stdin);

		if (MessageBoxExW(
			NULL,
			L"Attach a debugger now?",
			L"Windows CE Compatibility Layer",
			MB_YESNO,
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) == IDYES)
		{
			WCHAR VSJitDebugger[MAX_PATH], CmdLineBuf[256], SystemDirectory[MAX_PATH];
			PROCESS_INFORMATION Info = { };
			STARTUPINFO StartupInfo = { };

			GetSystemDirectoryW(SystemDirectory, MAX_PATH);
			swprintf_s(VSJitDebugger, MAX_PATH, L"%s\\vsjitdebugger.exe", SystemDirectory);
			swprintf_s(CmdLineBuf, 256, L"%s -p %d", VSJitDebugger, GetCurrentProcessId());

			StartupInfo.wShowWindow = TRUE;

			// MessageBoxW(NULL, CmdLineBuf, VSJitDebugger, 0);

			Assert32(CreateProcess(
				VSJitDebugger,
				CmdLineBuf,
				NULL, NULL,
				FALSE, NORMAL_PRIORITY_CLASS,
				NULL, NULL, &StartupInfo, &Info) == FALSE);

			WaitForSingleObject(Info.hProcess, -1);

			CloseHandle(Info.hProcess);
			CloseHandle(Info.hThread);
		}

		MODULEINFO moduleInfo;
		HMODULE hModule = GetModuleHandleW(NULL);

		GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));
		CE_ENTRYPOINT entrypoint = (CE_ENTRYPOINT)moduleInfo.EntryPoint;

		LPCWSTR wstr = L"";
		ExitProcess(entrypoint(hModule, NULL, wstr, 0));
		break;
	};
	//
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

