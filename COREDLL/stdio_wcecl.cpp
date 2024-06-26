#include "stdafx.h"
#include <assert.h>
#include <io.h>

static HANDLE ControlEvent = NULL;

/* https://learn.microsoft.com/en-us/windows/console/clearing-the-screen */
static BOOL WceclConsoleClearScreen(
	HANDLE hDevice)
{
	CHAR_INFO charInfo;
	COORD scrollTarget;
	SMALL_RECT scrollRect;
	CONSOLE_SCREEN_BUFFER_INFO screenInfo;

	if (!GetConsoleScreenBufferInfo(hDevice, &screenInfo))
	{
		return FALSE;
	}

	charInfo.Char.UnicodeChar = L' ';
	charInfo.Attributes = screenInfo.wAttributes;

	scrollRect.Left = 0;
	scrollRect.Top = 0;
	scrollRect.Right = screenInfo.dwSize.X;
	scrollRect.Bottom = screenInfo.dwSize.Y;

	scrollTarget.X = 0;
	scrollTarget.Y = -screenInfo.dwSize.Y;

	if (ScrollConsoleScreenBufferW(
			hDevice,
			&scrollRect,
			NULL,
			scrollTarget,
			&charInfo) == 0)
	{
		return FALSE;
	}

	screenInfo.dwCursorPosition.X = 0;
	screenInfo.dwCursorPosition.Y = 0;

	SetConsoleCursorPosition(hDevice, screenInfo.dwCursorPosition);
}

static DWORD GetWin32ConsoleModeFromWce(DWORD wceConsoleMode)
{
	DWORD out = 0;

	if (wceConsoleMode & CECONSOLE_MODE_ECHO_INPUT)
	{
		out |= ENABLE_ECHO_INPUT;
	}
	if (wceConsoleMode & CECONSOLE_MODE_LINE_INPUT)
	{
		out |= ENABLE_LINE_INPUT;
	}
	if (wceConsoleMode & CECONSOLE_MODE_PROCESSED_OUTPUT)
	{
		out |= ENABLE_PROCESSED_OUTPUT;
	}

	return out;
}

static DWORD GetWceConsoleModeFromWin32(DWORD win32ConsoleMode)
{
	DWORD out = 0;

	if (win32ConsoleMode & ENABLE_ECHO_INPUT)
	{
		out |= CECONSOLE_MODE_ECHO_INPUT;
	}
	if (win32ConsoleMode & ENABLE_LINE_INPUT)
	{
		out |= CECONSOLE_MODE_LINE_INPUT;
	}
	if (win32ConsoleMode & ENABLE_PROCESSED_OUTPUT)
	{
		out |= CECONSOLE_MODE_PROCESSED_OUTPUT;
	}

	return out;
}

static BOOL WceclConsoleSetMode(
	HANDLE hDevice,
	DWORD* lpInBuf,
	DWORD nInBufSize)
{
	if (nInBufSize < sizeof(DWORD))
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	return SetConsoleMode(
		hDevice,
		GetWin32ConsoleModeFromWce(*lpInBuf));
}

static BOOL WceclConsoleGetMode(
	HANDLE hDevice,
	DWORD* lpOutBuf,
	DWORD nOutBufSize)
{
	DWORD win32ConsoleMode;
	BOOL result;

	if (nOutBufSize < sizeof(DWORD))
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	result = GetConsoleMode(hDevice, &win32ConsoleMode);
	if (!result)
	{
		return FALSE;
	}
	*((DWORD*)lpOutBuf) = GetWceConsoleModeFromWin32(win32ConsoleMode);
	return TRUE;
}

static BOOL WceclConsoleSetTitle(
	LPCSTR lpInBuf,
	DWORD nInBufSize)
{
	BOOL result;
	
	/* Create a copy of the buffer to not read garbage if the original buffer 
	   is short. */
	char* bufferCopy = new char[nInBufSize + 1];
	bufferCopy[nInBufSize] = '\0';
	memcpy(bufferCopy, lpInBuf, nInBufSize);

	result = SetConsoleTitleA(bufferCopy);

	delete[] bufferCopy;
	return result;
}

static BOOL WceclConsoleGetTitle(
	LPSTR lpOutBuf,
	DWORD nOutBufSize)
{
	return GetConsoleTitleA(lpOutBuf, nOutBufSize);
}


static BOOL WceclConsoleGetRowsCols(
	HANDLE hDevice,
	PDWORD lpCols,
	PDWORD lpRows,
	DWORD nOutBufSize)
{
	CONSOLE_SCREEN_BUFFER_INFO screenInfo;
	DWORD win32ConsoleMode;
	BOOL result;

	if (nOutBufSize < sizeof(DWORD))
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	
	if (!GetConsoleScreenBufferInfo(hDevice, &screenInfo))
	{
		return FALSE;
	}

	if (lpRows != NULL)
	{
		*lpRows = screenInfo.dwSize.Y;
	}
	if (lpCols != NULL)
	{
		*lpCols = screenInfo.dwSize.X;
	}
	if (lpCols == NULL && lpRows == NULL)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL WceclConsoleSetControlHandler(PHANDLER_ROUTINE* pInBuffer, DWORD nInBufSize)
{
	if (nInBufSize < sizeof(PHANDLER_ROUTINE))
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	return SetConsoleCtrlHandler(*pInBuffer, FALSE);
}

BOOL __stdcall WceclConsoleSignalControlEvent(DWORD ctrlType)
{
	if (ControlEvent != NULL)
	{
		SetEvent(ControlEvent);

		/* TODO: what should this return? */
		return TRUE;
	}
	return FALSE;
}

BOOL WceclConsoleSetControlEvent(HANDLE* pInBuffer, DWORD nInBufSize)
{
	if (nInBufSize < sizeof(HANDLE))
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	ControlEvent = *pInBuffer;
	return SetConsoleCtrlHandler(WceclConsoleSignalControlEvent, FALSE);
}
BOOL WceclConsoleIoControl(
	HANDLE hDevice,
	DWORD dwIoControlCode,
	LPVOID lpInBuf,
	DWORD nInBufSize,
	LPVOID lpOutBuf,
	DWORD nOutBufSize,
	LPDWORD lpBytesReturned,
	LPOVERLAPPED lpOverlapped)
{
	switch (dwIoControlCode)
	{
	case IOCTL_CONSOLE_SETMODE:
		return WceclConsoleSetMode(hDevice, (DWORD*)lpInBuf, nInBufSize);
	case IOCTL_CONSOLE_GETMODE:
		return WceclConsoleGetMode(hDevice, (DWORD*)lpOutBuf, nOutBufSize);
	case IOCTL_CONSOLE_SETTITLE:
		return WceclConsoleSetTitle((LPCSTR)lpInBuf, nInBufSize);
	case IOCTL_CONSOLE_GETTITLE:
		return WceclConsoleGetTitle((LPSTR)lpOutBuf, nOutBufSize);
	case IOCTL_CONSOLE_CLS:
		return WceclConsoleClearScreen(hDevice);
	case IOCTL_CONSOLE_FLUSHINPUT:
		return FlushConsoleInputBuffer(hDevice);
	case IOCTL_CONSOLE_GETSCREENROWS:
		return WceclConsoleGetRowsCols(hDevice, NULL, (PDWORD)lpOutBuf, nOutBufSize);
	case IOCTL_CONSOLE_GETSCREENCOLS:
		return WceclConsoleGetRowsCols(hDevice, (PDWORD)lpOutBuf, NULL, nOutBufSize);
	case IOCTL_CONSOLE_SETCONTROLCHANDLER:
		return WceclConsoleSetControlHandler((PHANDLER_ROUTINE*)lpInBuf, nInBufSize);
	case IOCTL_CONSOLE_SETCONTROLCEVENT:
		return WceclConsoleSetControlEvent((PHANDLE)lpInBuf, nInBufSize);
	}

	return FALSE;
}

FILE* WINAPI _getstdfilex_WCECL(DWORD type)
{
	switch (type)
	{
	case 0:
		return stdin;
	case 1:
		return stdout;
	case 2:
		return stderr;
	default:
		Assert32(FALSE);
		return NULL;
	}
}

BOOL WINAPI SetStdioPathW_WCECL(
	DWORD id,
	PWSTR pwszPath)
{
	/* TODO: test */
	switch (id)
	{
	case 0:
		return (_wfreopen(pwszPath, L"r", stdin) != NULL);
	case 1:
		return (_wfreopen(pwszPath, L"w", stdout) != NULL);
	case 2:
		return (_wfreopen(pwszPath, L"w", stderr) != NULL);
	default:
		Assert32(FALSE);
		return NULL;
	}
}

BOOL WINAPI GetStdioPathW_WCECL(
	DWORD id,
	PWSTR pwszBuf,
	LPDWORD lpdwLen)
{
	/* TODO: test */
	FILE* filePtr = _getstdfilex_WCECL(id);
	HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(filePtr));
	if (GetFinalPathNameByHandleW(hFile, pwszBuf, *lpdwLen, 0) < *lpdwLen)
	{
		return TRUE;
	}

	return FALSE;
}

static BOOL WceclTryGetStdHandle(FILE* file, PHANDLE handle)
{
	if (file == stdin)
	{
		*handle = GetStdHandle(STD_INPUT_HANDLE);
	}
	else if (file == stdout)
	{
		*handle = GetStdHandle(STD_OUTPUT_HANDLE);
	}
	else if (file == stderr)
	{
		*handle = GetStdHandle(STD_ERROR_HANDLE);
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}

/* This is only really executed, if the WinCE application has been converted to 
   Win32 GUI application (WinCE has no distinction between GUI and CUI subsystems) */
static BOOL WceclAllocateStdio()
{
	HWND hWndConsole;
	BOOL bConsoleAllocated;

	HANDLE hOldStdIn, hOldStdOut, hOldStdErr;

	hWndConsole = GetConsoleWindow();

	hOldStdIn = GetStdHandle(STD_INPUT_HANDLE);
	hOldStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	hOldStdErr = GetStdHandle(STD_ERROR_HANDLE);

	if (hWndConsole == NULL)
	{
		AllocConsole();
		hWndConsole = GetConsoleWindow();

		if (hWndConsole == NULL)
		{
			return FALSE;
		}

		bConsoleAllocated = TRUE;
	}
	else
	{
		bConsoleAllocated = FALSE;
	}

	assert(hOldStdErr == NULL || hOldStdIn == NULL || hOldStdOut == NULL);

	if (hOldStdIn == NULL)
	{
		if (freopen("CONIN$", "r", stdin) == NULL)
		{
			goto CLEANUP;
		}
	}
	if (hOldStdOut == NULL)
	{
		if(freopen("CONOUT$", "w", stdout) == NULL)
		{
			goto CLEANUP;
		}
	}
	if (hOldStdErr == NULL)
	{
		if(freopen("CONERR$", "w", stderr) == NULL)
		{
			goto CLEANUP;
		}
	}

	return TRUE;
CLEANUP:
	if (bConsoleAllocated)
	{
		FreeConsole();
	}
	return FALSE;
}

/* It seems that CE programs launch a console only when it is about to be
   used. */
HANDLE WceclTryGetOrAllocStdHandle(FILE* file)
{
	HANDLE hFile;

	if (WceclTryGetStdHandle(file, &hFile) == FALSE)
	{
		return NULL;
	}

	if (hFile == NULL)
	{
		if (WceclAllocateStdio() == FALSE)
		{
			return FALSE;
		}
	}

	return hFile;
}