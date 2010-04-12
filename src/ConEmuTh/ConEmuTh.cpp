
/*
Copyright (c) 2009-2010 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifdef _DEBUG
//  �����������������, ����� ����� ����� �������� ������� �������� MessageBox, ����� ����������� ����������
//  #define SHOW_STARTED_MSGBOX
#endif

#define TRUE_COLORER_OLD_SUPPORT

#define SHOWDEBUGSTR
#define MCHKHEAP
#define DEBUGSTRMENU(s) DEBUGSTR(s)


//#include <stdio.h>
#include <windows.h>
//#include <windowsx.h>
//#include <string.h>
//#include <tchar.h>
#include "../common/common.hpp"
#include "../common/pluginW1007.hpp" // ���������� �� 995 �������� SynchoApi
#include "../common/RgnDetect.h"
#include "ConEmuTh.h"

#define Free free
#define Alloc calloc

#define MAKEFARVERSION(major,minor,build) ( ((major)<<8) | (minor) | ((build)<<16))

#define ConEmuTh_SysID 0x43455568 // 'CETh'

#ifdef _DEBUG
wchar_t gszDbgModLabel[6] = {0};
#endif

#if defined(__GNUC__)
extern "C"{
  BOOL WINAPI DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved );
  void WINAPI SetStartupInfoW(void *aInfo);
};
#endif


HWND ghConEmuRoot = NULL;
HMODULE ghPluginModule = NULL; // ConEmuTh.dll - ��� ������
BOOL TerminalMode = FALSE;
DWORD gnSelfPID = 0;
DWORD gnMainThreadId = 0;
HANDLE ghDisplayThread = NULL; DWORD gnDisplayThreadId = 0;
HWND ghLeftView = NULL, ghRightView = NULL;
WCHAR gszRootKey[MAX_PATH*2];
FarVersion gFarVersion;
//HMODULE ghConEmuDll = NULL;
typedef int (WINAPI *RegisterPanelView_t)(PanelViewInit *ppvi);
typedef HWND (WINAPI *GetFarHWND2_t)(BOOL abConEmuOnly);
RegisterPanelView_t gfRegisterPanelView = NULL;
GetFarHWND2_t gfGetFarHWND2 = NULL;
CeFullPanelInfo pviLeft = {0}, pviRight = {0};
int ShowLastError();
CRgnDetect *gpRgnDetect = NULL;
CEFAR_INFO gFarInfo;
bool gbLastCheckWindow = false;

ThumbnailSettings gThSet; // = {96,96,1, 4,20, 1,1, 14, L"Tahoma"};

bool gbWaitForKeySequenceEnd = false;
DWORD gnWaitForKeySeqTick = 0;
int gnUngetCount = 0;
INPUT_RECORD girUnget[100];
WORD wScanCodeUp=0, wScanCodeDown=0, wScanCodeLeft=0, wScanCodeRight=0; //p->Event.KeyEvent.wVirtualScanCode = MapVirtualKey(vk, 0/*MAPVK_VK_TO_VSC*/);
BOOL GetBufferInput(BOOL abRemove, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead);
BOOL UngetBufferInput(PINPUT_RECORD lpOneInput);
BOOL ProcessConsoleInput(BOOL abUseUngetBuffer, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead);

#define EVENT_TYPE_REDRAW 250
#define TH_ENVVAR_NAME L"FarThumbnails"
#define TH_ENVVAR_ACTIVE L"Active"
#define TH_ENVVAR_SCROLL L"Scrolling"


// minimal(?) FAR version 2.0 alpha build FAR_X_VER
int WINAPI _export GetMinFarVersionW(void)
{
	return MAKEFARVERSION(2,0,FAR_X_VER);
}

/* COMMON - ���� ��������� �� ����������� */
void WINAPI _export GetPluginInfoW(struct PluginInfo *pi)
{
    static WCHAR *szMenu[1], szMenu1[255];
    szMenu[0]=szMenu1;
	lstrcpynW(szMenu1, GetMsgW(0), 240);


	pi->StructSize = sizeof(struct PluginInfo);
	pi->Flags = 0; // PF_PRELOAD; //TODO: ��������� Preload, ���� ����� ����� ��������������� ��� ������
	pi->DiskMenuStrings = NULL;
	pi->DiskMenuNumbers = 0;
	pi->PluginMenuStrings = szMenu;
	pi->PluginMenuStringsNumber = 1;
	pi->PluginConfigStrings = NULL;
	pi->PluginConfigStringsNumber = 0;
	pi->CommandPrefix = 0;
	pi->Reserved = ConEmuTh_SysID; // 'CETh'
}


BOOL gbInfoW_OK = FALSE;
HANDLE WINAPI _export OpenPluginW(int OpenFrom,INT_PTR Item)
{
	if (!gbInfoW_OK || !CheckConEmu(TRUE))
		return INVALID_HANDLE_VALUE;
		
	gThSet.Load();
	// ��� �������� ������� - ��������� ���������� �� �������� ������, ��������� ������� ��� ������ ��� ��������
	CeFullPanelInfo* pi = LoadPanelInfo(TRUE);
	HWND hView = (pi->bLeftPanel) ? ghLeftView : ghRightView;
	
	PanelViewInit pvi = {sizeof(PanelViewInit)};
	pvi.bLeftPanel = pi->bLeftPanel;
	pvi.nFarInterfaceSettings = pi->nFarInterfaceSettings;
	pvi.nFarPanelSettings = pi->nFarPanelSettings;
	pvi.PanelRect = pi->PanelRect;
	//pvi.pfnReadCall = OnReadConsole;
	pvi.pfnPeekPreCall = OnPrePeekConsole;
	pvi.pfnPeekPostCall = OnPostPeekConsole;
	pvi.pfnReadPreCall = OnPreReadConsole;
	pvi.pfnReadPostCall = OnPostReadConsole;
	pvi.pfnWriteCall = OnPostWriteConsoleOutput;
	
	BOOL lbRc = FALSE;
	DWORD dwErr = 0;

	if (hView == NULL) {
		// ����� ������� View
		hView = CreateView(pi);
		if (hView == NULL) {
			// �������� ������
			ShowLastError();
		} else {
			// ������������������
			pvi.bRegister = TRUE;
			pvi.hWnd = hView;
			int nRegRC = 0;
			if ((nRegRC = RegisterPanelView(&pvi)) != 0) {
				// ������� ���� (�� WM_DESTROY ���� ���� ������ ������� WM_QUIT ���� ��� ������������)
				_ASSERTE(nRegRC == 0);
				PostMessage(hView, WM_DESTROY, 0, 0);
				gnCreateViewError = CEGuiDontAcceptPanel;
				gnWin32Error = nRegRC;
				ShowLastError();
			} else {
				lbRc = ShowWindow(hView, SW_SHOWNORMAL);
				if (!lbRc)
					dwErr = GetLastError();
				_ASSERTE(lbRc || IsWindowVisible(hView));

				UpdateEnvVar(FALSE);
			}
		}
	} else {
		// ������������������
		pvi.bRegister = FALSE;
		pvi.hWnd = hView;
		RegisterPanelView(&pvi);
		// ������� ���� (�� WM_CLOSE ���� ���� ������ ������� WM_QUIT ���� ��� ������������)
		PostMessage(hView, WM_CLOSE, 0, 0);
	}

	return INVALID_HANDLE_VALUE;
}



BOOL WINAPI DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			{
				ghPluginModule = (HMODULE)hModule;
				gnSelfPID = GetCurrentProcessId();
				gnMainThreadId = GetCurrentThreadId();
				
				_ASSERTE(FAR_X_VER<=FAR_Y_VER);
				#ifdef SHOW_STARTED_MSGBOX
				if (!IsDebuggerPresent()) MessageBoxA(NULL, "ConEmuTh*.dll loaded", "ConEmu Thumbnails", 0);
				#endif
				
			    // Check Terminal mode
			    TCHAR szVarValue[MAX_PATH];
			    szVarValue[0] = 0;
			    if (GetEnvironmentVariable(L"TERM", szVarValue, 63)) {
				    TerminalMode = TRUE;
			    }
			}
			break;
		case DLL_PROCESS_DETACH:
			//if (ghConEmuDll) {
			//	//
			//	TODO("��������� ���� � ������������������");
			//	//
			//	RegisterPanelView = NULL;
			//	GetFarHWND2 = NULL;
			//	FreeLibrary(ghConEmuDll); ghConEmuDll = NULL;
			//}
			break;
	}
	return TRUE;
}

#if defined(CRTSTARTUP)
extern "C"{
  BOOL WINAPI _DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved);
};

BOOL WINAPI _DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
  DllMain(hDll, dwReason, lpReserved);
  return TRUE;
}
#endif




BOOL LoadFarVersion()
{
	BOOL lbRc=FALSE;
	WCHAR FarPath[MAX_PATH+1];
	if (GetModuleFileName(0,FarPath,MAX_PATH)) {
		DWORD dwRsrvd = 0;
		DWORD dwSize = GetFileVersionInfoSize(FarPath, &dwRsrvd);
		if (dwSize>0) {
			void *pVerData = Alloc(dwSize, 1);
			if (pVerData) {
				VS_FIXEDFILEINFO *lvs = NULL;
				UINT nLen = sizeof(lvs);
				if (GetFileVersionInfo(FarPath, 0, dwSize, pVerData)) {
					TCHAR szSlash[3]; lstrcpyW(szSlash, L"\\");
					if (VerQueryValue ((void*)pVerData, szSlash, (void**)&lvs, &nLen)) {
						gFarVersion.dwVer = lvs->dwFileVersionMS;
						gFarVersion.dwBuild = lvs->dwFileVersionLS;
						lbRc = TRUE;
					}
				}
				Free(pVerData);
			}
		}
	}

	if (!lbRc) {
		gFarVersion.dwVerMajor = 2;
		gFarVersion.dwVerMinor = 0;
		gFarVersion.dwBuild = FAR_X_VER;
	}

	return lbRc;
}

#define CONEMUCHECKDELTA 2000
static DWORD nLastCheckConEmu = 0;
BOOL CheckConEmu(BOOL abForceCheck)
{
	// ��������� �� ����, ��� conemu.dll ����� ��������� (unload:...)
	if (!abForceCheck) {
		if ((GetTickCount() - nLastCheckConEmu) > CONEMUCHECKDELTA) {
			if (!ghConEmuRoot)
				return FALSE;
			return TRUE;
		}
	}

	nLastCheckConEmu = GetTickCount();
	HMODULE hConEmu = GetModuleHandle
		(
			#ifdef WIN64
					L"ConEmu.x64.dll"
			#else
					L"ConEmu.dll"
			#endif
		);
	if (!hConEmu) {
		ShowMessage(CEPluginNotFound, 0);
		return FALSE;
	}
	gfRegisterPanelView = (RegisterPanelView_t)GetProcAddress(hConEmu, "RegisterPanelView");
	gfGetFarHWND2 = (GetFarHWND2_t)GetProcAddress(hConEmu, "GetFarHWND2");
	if (!gfRegisterPanelView || !gfGetFarHWND2) {
		ShowMessage(CEOldPluginVersion, 0);
		return FALSE;
	}
	HWND hWnd = gfGetFarHWND2(TRUE);
	if (hWnd) ghConEmuRoot = GetParent(hWnd); else ghConEmuRoot = NULL;
	if (!hWnd) {
		ShowMessage(CEFarNonGuiMode, 0);
		return FALSE;
	}
	return TRUE;
}

int RegisterPanelView(PanelViewInit *ppvi)
{
	// ��������� �� ����, ��� conemu.dll ����� ��������� (unload:...)
	if (!CheckConEmu() || !gfRegisterPanelView)
		return -1;

	int nRc = gfRegisterPanelView(ppvi);

	return nRc;
}

HWND GetConEmuHWND()
{
	// ��������� �� ����, ��� conemu.dll ����� ��������� (unload:...)
	if (!CheckConEmu() || !gfGetFarHWND2)
		return NULL;

	HWND hRoot = NULL;
	HWND hConEmu = gfGetFarHWND2(TRUE);
	if (hConEmu)
		hRoot = GetParent(hConEmu);
	
	return hRoot;
}













void WINAPI _export SetStartupInfoW(void *aInfo)
{
	if (!gFarVersion.dwVerMajor) LoadFarVersion();

	if (gFarVersion.dwBuild>=FAR_Y_VER)
		FUNC_Y(SetStartupInfoW)(aInfo);
	else
		FUNC_X(SetStartupInfoW)(aInfo);

	gbInfoW_OK = TRUE;
}






	#ifndef max
	#define max(a,b)            (((a) > (b)) ? (a) : (b))
	#endif

	#ifndef min
	#define min(a,b)            (((a) < (b)) ? (a) : (b))
	#endif








void StopThread(void)
{
	if (ghLeftView)
		PostMessage(ghLeftView, WM_CLOSE,0,0);
	if (ghRightView)
		PostMessage(ghRightView, WM_CLOSE,0,0);
	DWORD dwWait = 0;
	if (ghDisplayThread)
		dwWait = WaitForSingleObject(ghDisplayThread, 500);
	if (dwWait)
		TerminateThread(ghDisplayThread, 100);
	if (ghDisplayThread) {
		CloseHandle(ghDisplayThread); ghDisplayThread = NULL;
	}
	gnDisplayThreadId = 0;
	// ���������� ������
	pviLeft.FreeInfo();
	pviRight.FreeInfo();
	if (gpRgnDetect) {
		delete gpRgnDetect;
		gpRgnDetect = NULL;
	}
}

void   WINAPI _export ExitFARW(void)
{
	StopThread();

	if (gFarVersion.dwBuild>=FAR_Y_VER)
		FUNC_Y(ExitFARW)();
	else
		FUNC_X(ExitFARW)();
}






int ShowMessage(int aiMsg, int aiButtons)
{
	if (gFarVersion.dwVerMajor==1)
		return ShowMessageA(aiMsg, aiButtons);
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		return FUNC_Y(ShowMessage)(aiMsg, aiButtons);
	else
		return FUNC_X(ShowMessage)(aiMsg, aiButtons);
}

LPCWSTR GetMsgW(int aiMsg)
{
	if (gFarVersion.dwVerMajor==1)
		return L"";
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		return FUNC_Y(GetMsg)(aiMsg);
	else
		return FUNC_X(GetMsg)(aiMsg);
}

void PostMacro(wchar_t* asMacro)
{
	if (!asMacro || !*asMacro)
		return;
		
	if (gFarVersion.dwVerMajor==1) {
		int nLen = lstrlenW(asMacro);
		char* pszMacro = (char*)Alloc(nLen+1,1);
		if (pszMacro) {
			WideCharToMultiByte(CP_OEMCP,0,asMacro,nLen+1,pszMacro,nLen+1,0,0);
			PostMacroA(pszMacro);
			Free(pszMacro);
		}
	} else if (gFarVersion.dwBuild>=FAR_Y_VER) {
		FUNC_Y(PostMacro)(asMacro);
	} else {
		FUNC_X(PostMacro)(asMacro);
	}

	//FAR BUGBUG: ������ �� ����������� �� ����������, ���� ������ �� ������ :(
	//  ��� ���� ����� ����������� ��� ������ ���� �� RClick
	//  ���� ������ �� ������ ������, �� RClick ����� �� ���������
	//  �� �������� ��������� :(
	//if (!mcr.Param.PlainText.Flags) {
	INPUT_RECORD ir[2] = {{MOUSE_EVENT},{MOUSE_EVENT}};
	if (isPressed(VK_CAPITAL))
		ir[0].Event.MouseEvent.dwControlKeyState |= CAPSLOCK_ON;
	if (isPressed(VK_NUMLOCK))
		ir[0].Event.MouseEvent.dwControlKeyState |= NUMLOCK_ON;
	if (isPressed(VK_SCROLL))
		ir[0].Event.MouseEvent.dwControlKeyState |= SCROLLLOCK_ON;
	ir[0].Event.MouseEvent.dwEventFlags = MOUSE_MOVED;
	ir[1].Event.MouseEvent.dwControlKeyState = ir[0].Event.MouseEvent.dwControlKeyState;
	ir[1].Event.MouseEvent.dwEventFlags = MOUSE_MOVED;
	ir[1].Event.MouseEvent.dwMousePosition.X = 1;
	ir[1].Event.MouseEvent.dwMousePosition.Y = 1;

	//2010-01-29 ��������� STD_OUTPUT
	//if (!ghConIn) {
	//	ghConIn  = CreateFile(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_READ,
	//		0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	//	if (ghConIn == INVALID_HANDLE_VALUE) {
	//		#ifdef _DEBUG
	//		DWORD dwErr = GetLastError();
	//		_ASSERTE(ghConIn!=INVALID_HANDLE_VALUE);
	//		#endif
	//		ghConIn = NULL;
	//		return;
	//	}
	//}
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	DWORD cbWritten = 0;
#ifdef _DEBUG
	BOOL fSuccess = 
#endif
	WriteConsoleInput(hIn/*ghConIn*/, ir, 1, &cbWritten);
	_ASSERTE(fSuccess && cbWritten==1);
	//}
	//InfoW995->AdvControl(InfoW995->ModuleNumber,ACTL_REDRAWALL,NULL);

}





void ShowPluginMenu(int nID /*= -1*/)
{
	//int nItem = -1;
	//if (!FarHwnd) {
	//	SHOWDBGINFO(L"*** ShowPluginMenu failed, FarHwnd is NULL\n");
	//	return;
	//}

	//if (nID != -1) {
	//	nItem = nID;
	//	if (nItem >= 2) nItem++; //Separator
	//	if (nItem >= 7) nItem++; //Separator
	//	if (nItem >= 9) nItem++; //Separator
	//	SHOWDBGINFO(L"*** ShowPluginMenu used default item\n");
	//} else if (gFarVersion.dwVerMajor==1) {
	//	SHOWDBGINFO(L"*** calling ShowPluginMenuA\n");
	//	nItem = ShowPluginMenuA();
	//} else if (gFarVersion.dwBuild>=FAR_Y_VER) {
	//	SHOWDBGINFO(L"*** calling ShowPluginMenuWY\n");
	//	nItem = FUNC_Y(ShowPluginMenu)();
	//} else {
	//	SHOWDBGINFO(L"*** calling ShowPluginMenuWX\n");
	//	nItem = FUNC_X(ShowPluginMenu)();
	//}

	//if (nItem < 0) {
	//	SHOWDBGINFO(L"*** ShowPluginMenu cancelled, nItem < 0\n");
	//	return;
	//}

	//#ifdef _DEBUG
	//wchar_t szInfo[128]; wsprintf(szInfo, L"*** ShowPluginMenu done, nItem == %i\n", nItem);
	//SHOWDBGINFO(szInfo);
	//#endif

	//switch (nItem) {
	//	case 0: case 1:
	//	{ // ������� � ��������� ����� ��������� ���������� ���������
	//		CESERVER_REQ* pIn = (CESERVER_REQ*)calloc(sizeof(CESERVER_REQ_HDR)+4,1);
	//		if (!pIn) return;
	//		CESERVER_REQ* pOut = NULL;
	//		ExecutePrepareCmd(pIn, CECMD_GETOUTPUTFILE, sizeof(CESERVER_REQ_HDR)+4);
	//		pIn->OutputFile.bUnicode = (gFarVersion.dwVerMajor>=2);
	//		pOut = ExecuteGuiCmd(FarHwnd, pIn, FarHwnd);
	//		if (pOut) {
	//			if (pOut->OutputFile.szFilePathName[0]) {
	//				BOOL lbRc = FALSE;
	//				if (gFarVersion.dwVerMajor==1)
	//					lbRc = EditOutputA(pOut->OutputFile.szFilePathName, (nItem==1));
	//				else if (gFarVersion.dwBuild>=FAR_Y_VER)
	//					lbRc = FUNC_Y(EditOutput)(pOut->OutputFile.szFilePathName, (nItem==1));
	//				else
	//					lbRc = FUNC_X(EditOutput)(pOut->OutputFile.szFilePathName, (nItem==1));
	//				if (!lbRc) {
	//					DeleteFile(pOut->OutputFile.szFilePathName);
	//				}
	//			}
	//			ExecuteFreeResult(pOut);
	//		}
	//		free(pIn);
	//	} break;
	//	case 3: // ��������/�������� ����
	//	case 4: case 5: case 6:
	//	{
	//		CESERVER_REQ in, *pOut = NULL;
	//		ExecutePrepareCmd(&in, CECMD_TABSCMD, sizeof(CESERVER_REQ_HDR)+1);
	//		in.Data[0] = nItem - 3;
	//		pOut = ExecuteGuiCmd(FarHwnd, &in, FarHwnd);
	//		if (pOut) ExecuteFreeResult(pOut);
	//	} break;
	//	case 8: // Attach to GUI (���� FAR ��� CtrlAltTab)
	//	{
	//		if (TerminalMode) break; // �����
	//		if (ConEmuHwnd && IsWindow(ConEmuHwnd)) break; // �� � ��� ����������?
	//		Attach2Gui();
	//	} break;
	//	case 10: // Start "ConEmuC.exe /DEBUGPID="
	//	{
	//		if (TerminalMode) break; // �����
	//		StartDebugger();
	//	}
	//}
}




BOOL IsMacroActive()
{
	BOOL lbActive = FALSE;
	if (gFarVersion.dwVerMajor==1)
		lbActive = IsMacroActiveA();
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		lbActive = FUNC_Y(IsMacroActive)();
	else
		lbActive = FUNC_X(IsMacroActive)();

	return lbActive;
}

void LoadPanelItemInfo(CeFullPanelInfo* pi, int nItem)
{
	if (gFarVersion.dwVerMajor==1)
		LoadPanelItemInfoA(pi, nItem);
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		FUNC_Y(LoadPanelItemInfo)(pi, nItem);
	else
		FUNC_X(LoadPanelItemInfo)(pi, nItem);
}


// ���������� (��� ��������) ������ �� ���� �� ���������� ���������� (pviLeft/pviRight)
CeFullPanelInfo* LoadPanelInfo(BOOL abActive)
{
	TODO("�������� ����� ACTL_GETWINDOW ���-��?");
	
	if (!gpRgnDetect) {
		gpRgnDetect = new CRgnDetect();
	}

	CeFullPanelInfo* ppi = NULL;

	if (gFarVersion.dwVerMajor==1)
		ppi = LoadPanelInfoA(abActive);
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		ppi = FUNC_Y(LoadPanelInfo)(abActive);
	else
		ppi = FUNC_X(LoadPanelInfo)(abActive);

	if (ppi)
		ppi->cbSize = sizeof(*ppi);

	return ppi;
}

void ReloadPanelsInfo()
{
	TODO("�������� ����� ACTL_GETWINDOW ���-��?");

	// ���� ��� � ������ ���� ������	
	if (!gpRgnDetect) {
		gpRgnDetect = new CRgnDetect();
	}

	if (gFarVersion.dwVerMajor==1)
		ReloadPanelsInfoA();
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		FUNC_Y(ReloadPanelsInfo)();
	else
		FUNC_X(ReloadPanelsInfo)();
}


BOOL IsLeftPanelActive()
{
	BOOL lbLeftActive = FALSE;
	if (gFarVersion.dwVerMajor==1)
		lbLeftActive = IsLeftPanelActiveA();
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		lbLeftActive = FUNC_Y(IsLeftPanelActive)();
	else
		lbLeftActive = FUNC_X(IsLeftPanelActive)();
	return lbLeftActive;
}


BOOL GetBufferInput(BOOL abRemove, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead)
{
	if (gnUngetCount<0) {
		_ASSERTE(gnUngetCount>=0);
		gnUngetCount = 0;
		*lpNumberOfEventsRead = 0;
		return FALSE;
	}

	BOOL lbRedraw = FALSE;

	// ���� ���������� �� ������� EVENT_TYPE_REDRAW - ���������� ���������� EnvVar
	// SetEnvironmentVariable(TH_ENVVAR_NAME, TH_ENVVAR_ACTIVE);
	if (girUnget[0].EventType == EVENT_TYPE_REDRAW)
	{
		lbRedraw = TRUE;
		gnUngetCount --;
		_ASSERTE(gnUngetCount >= 0);
		if (gnUngetCount > 0) {
			// ��������� � ������ �� ��� �������� � ������
			memmove(girUnget, girUnget+1, sizeof(girUnget[0])*gnUngetCount);
		}
		girUnget[gnUngetCount].EventType = 0;
	}

	int nMax = sizeofarray(girUnget);
	if (gnUngetCount>=nMax) {
		_ASSERTE(gnUngetCount==nMax);
		if (gnUngetCount>nMax) gnUngetCount = nMax;
	}

	nMax = min(gnUngetCount,(int)nBufSize);

	int i = 0, j = 0;
	while (i < nMax && j < gnUngetCount) {
		if (girUnget[j].EventType == EVENT_TYPE_REDRAW) {
			j++; lbRedraw = TRUE; continue;
		}

		lpBuffer[i++] = girUnget[j++];
	}
	nMax = i;

	if (abRemove) {
		gnUngetCount -= j;
		_ASSERTE(gnUngetCount >= 0);
		if (gnUngetCount > 0) {
			// ��������� � ������ �� ��� �������� � ������
			memmove(girUnget, girUnget+nMax, sizeof(girUnget[0])*gnUngetCount);
			girUnget[gnUngetCount].EventType = 0;
		}
	}

	if (lbRedraw) {
		gbWaitForKeySequenceEnd = (gnUngetCount > 0);
		if (IsThumbnailsActive(FALSE))
			UpdateEnvVar(TRUE);
	}

	*lpNumberOfEventsRead = nMax;

	return TRUE;
}

BOOL UngetBufferInput(PINPUT_RECORD lpOneInput)
{
	if (gnUngetCount<0) {
		_ASSERTE(gnUngetCount>=0);
		gnUngetCount = 0;
	}
	int nMax = sizeofarray(girUnget);
	if (gnUngetCount>=nMax) {
		_ASSERTE(gnUngetCount==nMax);
		if (gnUngetCount>nMax) gnUngetCount = nMax;
		return FALSE;
	}

	girUnget[gnUngetCount] = *lpOneInput;
	gnUngetCount ++;	

	return TRUE;
}


BOOL WINAPI OnPrePeekConsole(HANDLE hInput, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead, BOOL* pbResult)
{
	if (!lpBuffer || !lpNumberOfEventsRead) {
		*pbResult = FALSE;
		return FALSE; // ������ � ����������
	}

	// ���� � girUnget ���-�� ���� ������� �� ���� � FALSE (����� OnPostPeekConsole �� ����� ������)");
	if (gnUngetCount) {
		if (GetBufferInput(FALSE/*abRemove*/, lpBuffer, nBufSize, lpNumberOfEventsRead))
			return FALSE; // PeekConsoleInput & OnPostPeekConsole �� ����� ������
	}

	return TRUE; // ���������� ��� ���������
}

BOOL WINAPI OnPostPeekConsole(HANDLE hInput, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead, BOOL* pbResult)
{
	if (!lpBuffer || !lpNumberOfEventsRead) {
		*pbResult = FALSE;
		return FALSE; // ������ � ����������
	}

	// �������� ��������� �������, �� girUnget �� �������
	if (*lpNumberOfEventsRead) {
		if (!ProcessConsoleInput(FALSE/*abUseUngetBuffer*/, lpBuffer, nBufSize, lpNumberOfEventsRead)) {
			if (*lpNumberOfEventsRead == 0) {
				*pbResult = FALSE;
			}
			return FALSE; // ��������� � ���������� �������
		}
	}
	
	return TRUE; // ���������� ��� ���������
}

BOOL WINAPI OnPreReadConsole(HANDLE hInput, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead, BOOL* pbResult)
{
	if (!lpBuffer || !lpNumberOfEventsRead) {
		*pbResult = FALSE;
		return FALSE; // ������ � ����������
	}

	// ���� � girUnget ���-�� ���� ������� �� ���� � FALSE (����� OnPostPeekConsole �� ����� ������)");
	if (gnUngetCount) {
		if (GetBufferInput(TRUE/*abRemove*/, lpBuffer, nBufSize, lpNumberOfEventsRead))
			return FALSE; // ReadConsoleInput & OnPostReadConsole �� ����� ������
	}

	return TRUE; // ���������� ��� ���������
}

BOOL WINAPI OnPostReadConsole(HANDLE hInput, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead, BOOL* pbResult)
{
	if (!lpBuffer || !lpNumberOfEventsRead) {
		*pbResult = FALSE;
		return FALSE; // ������ � ����������
	}

	// �������� ��������� �������, ���� ���� - ��������� ����� ������� � girUnget
	if (*lpNumberOfEventsRead) {
		if (!ProcessConsoleInput(TRUE/*abUseUngetBuffer*/, lpBuffer, nBufSize, lpNumberOfEventsRead)) {
			if (*lpNumberOfEventsRead == 0) {
				*pbResult = FALSE;
			}
			return FALSE; // ��������� � ���������� �������
		}
	}
	
	return TRUE; // ���������� ��� ���������
}




//lpBuffer 
//The data to be written to the console screen buffer. This pointer is treated as the origin of a 
//two-dimensional array of CHAR_INFO structures whose size is specified by the dwBufferSize parameter. 
//The total size of the array must be less than 64K.
//
//dwBufferSize 
//The size of the buffer pointed to by the lpBuffer parameter, in character cells. 
//The X member of the COORD structure is the number of columns; the Y member is the number of rows.
//
//dwBufferCoord 
//The coordinates of the upper-left cell in the buffer pointed to by the lpBuffer parameter. 
//The X member of the COORD structure is the column, and the Y member is the row.
//
//lpWriteRegion 
//A pointer to a SMALL_RECT structure. On input, the structure members specify the upper-left and lower-right 
//coordinates of the console screen buffer rectangle to write to. 
//On output, the structure members specify the actual rectangle that was used.
VOID WINAPI OnPostWriteConsoleOutput(HANDLE hOutput,const CHAR_INFO *lpBuffer,COORD dwBufferSize,COORD dwBufferCoord,PSMALL_RECT lpWriteRegion)
{
	if (gpRgnDetect && lpBuffer && lpWriteRegion) {
		gpRgnDetect->OnWriteConsoleOutput(lpBuffer, dwBufferSize, dwBufferCoord, lpWriteRegion);
	}
	
	ReloadPanelsInfo();
	
	if (!CheckWindows()) {
		// ��������/�����������������?
	} else {
		if (pviLeft.hView || pviRight.hView) {
			CeFullPanelInfo* p = pviLeft.hView ? &pviLeft : &pviRight;

			gFarInfo.FarVer = gFarVersion;
			gFarInfo.nFarPID = GetCurrentProcessId();
			gFarInfo.nFarTID = GetCurrentThreadId();
			int n = min(p->nMaxFarColors, sizeofarray(gFarInfo.nFarColors));
			if (n && p->nFarColors) memmove(gFarInfo.nFarColors, p->nFarColors, n);
			gFarInfo.nFarInterfaceSettings = p->nFarInterfaceSettings;
			gFarInfo.nFarPanelSettings = p->nFarPanelSettings;
			gFarInfo.nFarConfirmationSettings = 0;
			gFarInfo.bFarPanelAllowed = TRUE;
			gFarInfo.bFarPanelInfoFilled = TRUE;

			gFarInfo.bFarLeftPanel = (pviLeft.Visible!=0);
			gFarInfo.FarLeftPanel.PanelRect = pviLeft.PanelRect;

			gFarInfo.bFarRightPanel = (pviRight.Visible!=0);
			gFarInfo.FarRightPanel.PanelRect = pviRight.PanelRect;

			HWND hWnd = p->hView;
			COLORREF crColors[16];
			for (int i=0; i<16; i++)
				crColors[i] = GetWindowLong(hWnd, 4*i);

			gpRgnDetect->PrepareTransparent(&gFarInfo, crColors);
		}
		
		if (!gbWaitForKeySequenceEnd)
		{
			if (pviLeft.hView)
				InvalidateRect(pviLeft.hView, NULL, FALSE);
			if (pviRight.hView)
				InvalidateRect(pviRight.hView, NULL, FALSE);
		}
	}

	//WARNING("��������� ������ ���� FAR � ���� �������� �� ������ - ����� ����� (� �������� �����������������)");
	//WARNING("��������� � ��������, ���� ��������� ������ ������ - �������� PanelView");

	//!. ������� ��������� �� ����� ������ ��������� �����!
	//   ��� ��������� ��� ����, ����� ���������� ������ �� ������������� �������
	//�����
	//1. ������ ������ -> ������� ���������� � ��������������, ��������� ������, � ��������� ���������
	//   ���������� � �������������� (���� �� ���������) �������� � GUI
	//   ���� ������ ������ �������� - ��������� ��������� ����������� � GUI - ��� ���� ������� ShowWindow
	//2. ������ �������� -> �������� ������ ������ � ������������������� � GUI
}


//// ������� TRUE, ���� ��� Unget
//BOOL ProcessKeyPress(CeFullPanelInfo* pi, BOOL abUseUngetBuffer, PINPUT_RECORD lpBuffer)
//{
//	_ASSERTE(lpBuffer->EventType == KEY_EVENT);
//	
//	// ��������������� �������
//	WORD vk = lpBuffer->Event.KeyEvent.wVirtualKeyCode;
//	BOOL lbWasUnget = FALSE;
//	
//	if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT)
//	{
//		if (!lpBuffer->Event.KeyEvent.bKeyDown) {
//			switch (vk) {
//				case VK_LEFT:  lpBuffer->Event.KeyEvent.wVirtualKeyCode = VK_UP;   break;
//				case VK_RIGHT: lpBuffer->Event.KeyEvent.wVirtualKeyCode = VK_DOWN; break;
//			}
//		
//		} else {
//			// ������������
//			int n = 0;
//			switch (vk) {
//				case VK_UP: {
//					n = min(pi->CurrentItem,pi->nXCount);
//				} break;
//				case VK_DOWN: {
//					n = min((pi->ItemsNumber-pi->CurrentItem-1),pi->nXCount);
//				} break;
//				case VK_LEFT: {
//					lpBuffer->Event.KeyEvent.wVirtualKeyCode = VK_UP;
//				} break;
//				case VK_RIGHT: {
//					lpBuffer->Event.KeyEvent.wVirtualKeyCode = VK_DOWN;
//				} break;
//			}
//			
//			lbWasUnget = (n > 1);
//			while ((n--) > 1) {
//				UngetBufferInput(lpBuffer);
//			}
//		}
//	}
//	return lbWasUnget;
//}

// ��������� ������ ������ � ��������� � ����� (Unget) ��������������� � lpBuffer �������
// ������� FALSE, ���� ������ ���� �����������
BOOL ProcessConsoleInput(BOOL abUseUngetBuffer, PINPUT_RECORD lpBuffer, DWORD nBufSize, LPDWORD lpNumberOfEventsRead)
{
	// �������� ���������� ��������
	CeFullPanelInfo* pi = IsThumbnailsActive(TRUE/*abFocusRequired*/);
	if (!pi) {
		return TRUE; // ������ �������, �� ��� �� ������� ��� ������ �������� ��� �������� ��������� ������
	}


	if (!wScanCodeUp) {
		wScanCodeUp = MapVirtualKey(VK_UP, 0/*MAPVK_VK_TO_VSC*/);
		wScanCodeDown = MapVirtualKey(VK_DOWN, 0/*MAPVK_VK_TO_VSC*/);
		wScanCodeLeft = MapVirtualKey(VK_LEFT, 0/*MAPVK_VK_TO_VSC*/);
		wScanCodeRight = MapVirtualKey(VK_RIGHT, 0/*MAPVK_VK_TO_VSC*/);
	}
	
	WARNING("��������� ���� �� DWORD-�� ���� �� ������� ������� ��������");
	WARNING("������� ��� ������ GUI");
	WARNING("���� ���� ���� �� ���� ������ - ������ ������������� ������");

	// ������ � ��� �������. � ������ - ��������� �����, � ��������� � ����� Unget.
	PINPUT_RECORD p = lpBuffer;
	PINPUT_RECORD pEnd = lpBuffer+*lpNumberOfEventsRead;
	PINPUT_RECORD pFirstReplace = NULL;
	while (p != pEnd)
	{
		if (p->EventType == KEY_EVENT)
		{
			// ��������������� �������
			WORD vk = p->Event.KeyEvent.wVirtualKeyCode;
			if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT)
			{
				// ������������
				int n = 1;
				switch (vk) {
					case VK_UP: {
						n = min(pi->CurrentItem,pi->nXCount);
								} break;
					case VK_DOWN: {
						n = min((pi->ItemsNumber-pi->CurrentItem-1),pi->nXCount);
								  } break;
					case VK_LEFT: {
						p->Event.KeyEvent.wVirtualKeyCode = VK_UP;
						p->Event.KeyEvent.wVirtualScanCode = wScanCodeUp;
								  } break;
					case VK_RIGHT: {
						p->Event.KeyEvent.wVirtualKeyCode = VK_DOWN;
						p->Event.KeyEvent.wVirtualScanCode = wScanCodeDown;
								   } break;
				}

				// ���� ��� ������� - �� �������������� ����� ��������� � Unget �����
				if (p->Event.KeyEvent.bKeyDown) {
					if (abUseUngetBuffer && n > 1) {
						pFirstReplace = p+1;
						for (int i = 2; i <= n; i++)
							UngetBufferInput(p);
					}
				}

				//end: if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT)
				p++; continue;
			}

			//end: if (p->EventType == KEY_EVENT)
		} else
		
		// ��� ����� ������� ���� - ����� ����������� ��������
		if (p->EventType == WINDOW_BUFFER_SIZE_EVENT) {
			if (gpRgnDetect)
				gpRgnDetect->OnWindowSizeChanged();
		}

		// ���� ���� �������� ������� � ����� Unget - ��������� ��������� � ����� � ��� ��� ���� �� ���,
		// �.�. ����� ���� ����� "�����������" ������� �������
		if (pFirstReplace && abUseUngetBuffer) {
			UngetBufferInput(p);
		}
		p++; continue;
	}
	// ��������������� ���������� "���������" �������
	if (pFirstReplace && abUseUngetBuffer)
	{
		DWORD nReady = pFirstReplace - lpBuffer;
		if (nReady != *lpNumberOfEventsRead) {
			_ASSERTE(nReady <= nBufSize);
			_ASSERTE(nReady < *lpNumberOfEventsRead);
			*lpNumberOfEventsRead = nReady;
		}
	}

	// ���� � ������ ���� "����������" ������� - �������� ����������� ��������� ���������� ���������
	if (pFirstReplace && abUseUngetBuffer && gnUngetCount) {
		_ASSERTE(gnUngetCount>0);
		INPUT_RECORD r = {EVENT_TYPE_REDRAW};
		UngetBufferInput(&r);
		//SetEnvironmentVariable(TH_ENVVAR_NAME, TH_ENVVAR_SCROLL);
		gbWaitForKeySequenceEnd = true;
		UpdateEnvVar(FALSE);
	}

	// ������ ������ - ���� ����� ���������� ������� - �������� � ���� ������� �� Unget ������
	if (gnUngetCount && nBufSize > *lpNumberOfEventsRead)
	{
		DWORD nAdd = 0;
		if (GetBufferInput(abUseUngetBuffer/*abRemove*/, pFirstReplace, nBufSize-(*lpNumberOfEventsRead), &nAdd))
			*lpNumberOfEventsRead += nAdd;
	}

	return TRUE;
}

int ShowLastError()
{
	if (gnCreateViewError) {
		wchar_t szErrMsg[512];
		const wchar_t* pszTempl = GetMsgW(gnCreateViewError);
		if (pszTempl && *pszTempl) {
			wsprintfW(szErrMsg, pszTempl, gnWin32Error);
			if (gFarVersion.dwBuild>=FAR_Y_VER)
				return FUNC_Y(ShowMessage)(szErrMsg, 0);
			else
				return FUNC_X(ShowMessage)(szErrMsg, 0);
		}
	}
	return 0;
}

void UpdateEnvVar(BOOL abForceRedraw)
{
	if (gbWaitForKeySequenceEnd) {
		SetEnvironmentVariable(TH_ENVVAR_NAME, TH_ENVVAR_SCROLL);
	} else if (IsThumbnailsActive(FALSE/*abFocusRequired*/)) {
		SetEnvironmentVariable(TH_ENVVAR_NAME, TH_ENVVAR_ACTIVE);
		abForceRedraw = TRUE;
	} else {
		SetEnvironmentVariable(TH_ENVVAR_NAME, NULL);
	}

	if (abForceRedraw) {
		if (pviLeft.hView && IsWindowVisible(pviLeft.hView))
			InvalidateRect(pviLeft.hView, NULL, FALSE);
		if (pviRight.hView && IsWindowVisible(pviRight.hView))
			InvalidateRect(pviRight.hView, NULL, FALSE);
	}
}

CeFullPanelInfo* IsThumbnailsActive(BOOL abFocusRequired)
{
	if (pviLeft.hView == NULL && pviRight.hView == NULL)
		return NULL;

	if (!gbLastCheckWindow)
		return NULL;

	CeFullPanelInfo* pi = NULL;
	
	if (gpRgnDetect) {
		DWORD dwFlags = gpRgnDetect->GetFlags();
		if ((dwFlags & FR_ACTIVEMENUBAR) == FR_ACTIVEMENUBAR)
			return NULL; // ������� ����
		if ((dwFlags & FR_FREEDLG_MASK) != 0)
			return NULL; // ���� �����-�� ������
	}

	if (abFocusRequired) {
		if (pviLeft.hView && pviLeft.Focus && pviLeft.Visible)
			pi = &pviLeft;
		else if (pviRight.hView && pviRight.Focus && pviRight.Visible)
			pi = &pviRight;
		// �����?
		if (pi) {
			if (!pi->hView || !IsWindowVisible(pi->hView)) {
				return NULL; // ������ ��������� ������ �������� ��� �������� ��������� ������
			}
		}
	} else {
		if (pviLeft.hView && IsWindowVisible(pviLeft.hView))
			pi = &pviLeft;
		else if (pviRight.hView && IsWindowVisible(pviRight.hView))
			pi = &pviRight;
	}
	

	return pi;
}

bool CheckWindows()
{
	if (gFarVersion.dwVerMajor==1)
		gbLastCheckWindow = CheckWindowsA();
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		gbLastCheckWindow = FUNC_Y(CheckWindows)();
	else
		gbLastCheckWindow = FUNC_X(CheckWindows)();
	return gbLastCheckWindow;
}
