/*
 * CLCLSet
 *
 * CLCLSet.c
 *
 * Copyright (C) 1996-2019 by Ohno Tomoaki. All rights reserved.
 *		https://www.nakka.com/
 *		nakka@nakka.com
 */

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include "..\General.h"

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	HWND main_window = FindWindow(MAIN_WND_CLASS, MAIN_WINDOW_TITLE);
	TCHAR directory[MAX_PATH], executable[MAX_PATH];
	TCHAR *separator;
	HINSTANCE result;
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);
	if (main_window != NULL) {
		PostMessage(main_window, WM_OPTION_SHOW, 0, 0);
		return 0;
	}
	GetModuleFileName(NULL, directory, ARRAYSIZE(directory));
	separator = _tcsrchr(directory, TEXT('\\'));
	if (separator == NULL) return 1;
	*separator = 0;
	if (lstrlen(directory) + lstrlen(MAIN_EXE) + 2 >= MAX_PATH) return 1;
	wsprintf(executable, TEXT("%s\\%s"), directory, MAIN_EXE);
	result = ShellExecute(NULL, TEXT("open"), executable, TEXT("/cmsettings"), directory, SW_SHOWNORMAL);
	if ((INT_PTR)result <= 32) {
		MessageBox(NULL, TEXT("ClipMaster\ub97c \uc2dc\uc791\ud560 \uc218 \uc5c6\uc2b5\ub2c8\ub2e4."), TEXT("ClipMaster"), MB_OK | MB_ICONERROR);
		return 1;
	}
	return 0;
}