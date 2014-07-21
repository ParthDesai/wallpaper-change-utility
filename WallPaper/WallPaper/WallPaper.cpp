// WallPaper.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "WallPaper.h"
#include "resource.h"
#define MAX_LOADSTRING 100
#define IDS_ALLOCATEMEM 2
#define UNIT_SECOND 4
#define UNIT_MINUTE 8
#define UNIT_HOUR 16
#define UNIT_DAYS 32
#define TIMER_THRESHOLD 300000
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define SZ_INSTRUCT TEXT("Select WallPaper Location")
LPTSTR lpWallPaperChangeTimeValueName = TEXT("WallPaper Change Time");
LPTSTR lpWallPaperDirectoryPathValueName = TEXT("WallPaper Path");
LPTSTR lpWallPaperChangeTimeUnitValueName = TEXT("Unit Of Time");
LPTSTR lpSecondUnit = TEXT("Second(s)");
LPTSTR lpDayUnit = TEXT("Day(s)");
LPTSTR lpHourUnit = TEXT("Hour(s)");
LPTSTR lpMinuteUnit = TEXT("Minute(s)");
LPTSTR lpAppName = TEXT("WallPaper Swap.exe");
LPTSTR lpControlMutex = TEXT("{59D657CE-8BB5-4024-AAA7-3A33181B1881}");
LPTSTR lpWallPaperSwapMutex = TEXT("{46128269-7D13-46DA-8607-4F5C9D3A2CDA}");
LPTSTR lpAppStartedMutex = TEXT("{8907782E-F821-4A4B-B247-85BB1868135B}");
LPTSTR lpTempTimeHolderValueName = TEXT("TempTimeHold");
LPTSTR lpCurrentWallPaperFileName = TEXT("CurrentWallPaper");
HANDLE hDataAccessMutex = CreateMutex(NULL,FALSE,NULL);
LPTSTR lpSupportedExtensionList[] = {TEXT("jpeg"),TEXT("jpg"),TEXT("png"),TEXT("gif"),TEXT("bmp"),NULL};
LPTSTR lpControlCommand = TEXT("/Control");
LPTSTR lpSwapCommand = TEXT("/Swap");
LPTSTR lpUninstallCommand = TEXT("/Uninstall");
LPTSTR lpRunValueName = TEXT("WallPaper Swap");
LPTSTR lpRunKey = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
LPTSTR lpAppInstallPath = NULL;
// For The POSIX name for this item is deprecated warning removal.
#define wcsicmp _wcsicmp
HANDLE FindFirstFileCust(LPCTSTR lpFolderPath,LPWIN32_FIND_DATA lpFindData);
struct StatusInfo
{
	BOOL bWallPaperSwapInstanceStarted;
	BOOL bControlInstanceStarted;
	BOOL bSwappingIsOn;
};
struct GUIControl
{
	HWND hEditControl;
	HWND hButtonControl;
	struct StatusInfo * SInfo;
};
struct TimeInfo
{
	union
	{
		DWORD dwDay;
		DWORD dwSecond;
		DWORD dwMinute;
		DWORD dwHour;
	}Time;
	DWORD dwTimeUnit;
};
void PrintError(__in DWORD dwError,__in_opt HWND hwnd)
{
	LPTSTR lpString;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL),
		(LPWSTR)&lpString,200,NULL);
	MessageBox(hwnd,lpString,TEXT("WallPaperSwap"),MB_OK | MB_ICONERROR);
}
struct WallPaperSettings
{
	DWORD dwChangeTime;
	LPTSTR lpDirectoryPath;
	DWORD dwUnitOfTime;
};
BOOL InitDefaultSettings(__in WallPaperSettings * wSettings,__in DWORD dwFlags,__in LPDWORD lpdwLenthOfString)
{
	if(!wSettings || !lpdwLenthOfString)
	{
		return FALSE;
	}
	if(dwFlags > 2)
	{
		PrintError(ERROR_INVALID_PARAMETER,NULL);
		exit(1);
	}
	static LPTSTR lpMyPicturesPath = NULL;
	lpMyPicturesPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!lpMyPicturesPath)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(SHGetFolderPath(NULL,CSIDL_MYPICTURES,NULL,SHGFP_TYPE_CURRENT,lpMyPicturesPath) != S_OK)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if((dwFlags & IDS_ALLOCATEMEM) == IDS_ALLOCATEMEM)
	{
		wSettings->lpDirectoryPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (wcslen(lpMyPicturesPath) + 1),MEM_COMMIT,PAGE_READWRITE);
		if(!wSettings->lpDirectoryPath)
		{
			PrintError(GetLastError(),NULL);
			exit(1);
		}
		*lpdwLenthOfString = wcslen(lpMyPicturesPath) + 1;
	}
	if(*lpdwLenthOfString < (wcslen(lpMyPicturesPath) + 1))
	{
		PrintError(ERROR_INSUFFICIENT_BUFFER,NULL);
		exit(1);
	}
	memcpy(wSettings->lpDirectoryPath,lpMyPicturesPath,wcslen(lpMyPicturesPath) * sizeof(TCHAR) + 1);
	wSettings->dwChangeTime = 5;
	wSettings->dwUnitOfTime = UNIT_SECOND;
	VirtualFree(lpMyPicturesPath,0,MEM_RELEASE);  
	return TRUE;
}
HINSTANCE GetThisModuleInstance(__in_opt HINSTANCE * hInst)
{
	static HINSTANCE hGlobalInstance = NULL;
	if(!hGlobalInstance)
	{
		if(!hInst)
		{
			return NULL;
		}
		hGlobalInstance = *hInst;
	}
	return hGlobalInstance;
}
BOOL GetThisModuleFileName(__in_opt TCHAR ** FileNamePointer)
{
	static TCHAR * FileName = NULL;
	
	if(!FileName)
	{
		FileName = (TCHAR *) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
		GetModuleFileName((HMODULE)GetThisModuleInstance(NULL),FileName,MAX_PATH);
	}
	if(FileNamePointer)
	{
		*FileNamePointer = FileName;
	}
	return TRUE;
}
		
BOOL IsThisFirstTime()
{
	HKEY hkey;
	LPTSTR lpRegKey = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!lpRegKey)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_REGKEY,lpRegKey,MAX_PATH))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(RegOpenKeyEx(HKEY_CURRENT_USER,lpRegKey,0,KEY_READ,&hkey) == ERROR_SUCCESS)
	{
		RegCloseKey(hkey);
		VirtualFree(lpRegKey,0,MEM_RELEASE);
		return FALSE;
	}
	else
	{
		VirtualFree(lpRegKey,0,MEM_RELEASE);
		return TRUE;
	}
}
BOOL InitApp(HINSTANCE hAppInst)
{
	HRESULT hRes;
	GetThisModuleInstance(&hAppInst);
	hRes = CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
	LPTSTR lpAppName = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * 250,MEM_COMMIT,PAGE_READWRITE);
	if(!lpAppName)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!LoadString(hAppInst,IDS_APPNAME,lpAppName,250))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(FAILED(hRes))
	{
		PrintError(HRESULT_CODE(hRes),NULL);
		exit(1);
	}
	DWORD dwSpaceForPath = ExpandEnvironmentStrings(TEXT("%APPDATA%"),lpAppInstallPath,0);
	DWORD dwExtraSpaceneeded = 40;
	lpAppInstallPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (dwSpaceForPath + dwExtraSpaceneeded),MEM_COMMIT,PAGE_READWRITE);
	if(!lpAppInstallPath)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!ExpandEnvironmentStrings(TEXT("%APPDATA%"),lpAppInstallPath,dwSpaceForPath + dwExtraSpaceneeded))
	{
		PrintError(GetLastError(),NULL);
	}
	wcscat_s(lpAppInstallPath,dwSpaceForPath + dwExtraSpaceneeded,TEXT("\\"));
	wcscat_s(lpAppInstallPath,dwSpaceForPath + dwExtraSpaceneeded,lpAppName);
	wcscat_s(lpAppInstallPath,dwSpaceForPath + dwExtraSpaceneeded,TEXT("\\"));
	wcscat_s(lpAppInstallPath,dwSpaceForPath + dwExtraSpaceneeded,lpAppName);
	wcscat_s(lpAppInstallPath,dwSpaceForPath + dwExtraSpaceneeded,TEXT(".exe"));
	return TRUE;
}
BOOL WriteSettingsIntoRegistry(WallPaperSettings * wSettings,DWORD dwStringLenth,HWND hwnd)
{
	if(!wSettings)
	{
		PrintError(ERROR_INVALID_PARAMETER,hwnd);
		exit(1);
	}
	if(wSettings->dwUnitOfTime != UNIT_SECOND && wSettings->dwUnitOfTime != UNIT_MINUTE && wSettings->dwUnitOfTime != UNIT_HOUR && wSettings->dwUnitOfTime != UNIT_DAYS)
	{
		PrintError(ERROR_INVALID_PARAMETER,hwnd);
		exit(1);
	}
	TCHAR * RegKey = (TCHAR *) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!RegKey)
	{
		PrintError(GetLastError(),hwnd);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_REGKEY,RegKey,MAX_PATH))
	{
		PrintError(GetLastError(),hwnd);
		exit(1);
	}
	DWORD dwLenthOfDirectoryPath = (dwStringLenth) ? (dwStringLenth * sizeof(TCHAR) + sizeof(TCHAR)) : wcslen(wSettings->lpDirectoryPath) * sizeof(TCHAR) + sizeof(TCHAR);
	HKEY hKey = NULL;
	LONG lResult;
	if((lResult = RegOpenKeyEx(HKEY_CURRENT_USER,RegKey,0,KEY_SET_VALUE,&hKey)) != ERROR_SUCCESS)
	{
		PrintError(lResult,hwnd);
		exit(1);
	}
	if((lResult = RegSetValueEx(hKey,lpWallPaperChangeTimeValueName,0,REG_DWORD,(const BYTE *)&wSettings->dwChangeTime,sizeof(DWORD))) != ERROR_SUCCESS)
	{
		PrintError(lResult,hwnd);
		exit(1);
	}
	if((lResult = RegSetValueEx(hKey,lpWallPaperDirectoryPathValueName,0,REG_SZ,(const BYTE *)wSettings->lpDirectoryPath,dwLenthOfDirectoryPath)) != ERROR_SUCCESS)
	{
		PrintError(lResult,hwnd);
		exit(1);
	}
	if((lResult = RegSetValueEx(hKey,lpWallPaperChangeTimeUnitValueName,0,REG_DWORD,(const BYTE *)&wSettings->dwUnitOfTime,sizeof(DWORD))) != ERROR_SUCCESS)
	{
		PrintError(lResult,hwnd);
		exit(1);
	}
	VirtualFree(RegKey,0,MEM_RELEASE);
	return TRUE;
}
BOOL ReadCurrentSettingsFromRegistry(WallPaperSettings * wSettings,LPDWORD lpdwLenthOfDirectoryPath,HWND hwnd)
{
	DWORD dwTypeOfValue = 0;
	DWORD dwSizeOfData = 0;
	DWORD dwLenthOfString = 0;
	LONG lResult;
	if(!wSettings)
	{
		PrintError(ERROR_INVALID_PARAMETER,hwnd);
		exit(1);
	}
	if(wSettings->lpDirectoryPath)
	{
		if(!lpdwLenthOfDirectoryPath)
		{
			PrintError(ERROR_INVALID_PARAMETER,hwnd);
			exit(1);
		}
		if(!(*lpdwLenthOfDirectoryPath))
		{
			PrintError(ERROR_INVALID_PARAMETER,hwnd);
			exit(1);
		}
	}
	TCHAR * lpRegKey = (TCHAR *) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!lpRegKey)
	{
		PrintError(GetLastError(),hwnd);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_REGKEY,lpRegKey,MAX_PATH))
	{
		PrintError(GetLastError(),hwnd);
		exit(1);
	}
	HKEY hKey;
	if((lResult = RegOpenKeyEx(HKEY_CURRENT_USER,lpRegKey,0,KEY_READ,&hKey)) != ERROR_SUCCESS)
	{
		PrintError(lResult,hwnd);
		exit(1);
	}
	dwSizeOfData = sizeof(DWORD);
	if((lResult = RegQueryValueEx(hKey,lpWallPaperChangeTimeValueName,NULL,&dwTypeOfValue,(LPBYTE)&(wSettings->dwChangeTime),&dwSizeOfData)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		PrintError(lResult,hwnd);
		exit(1);
	}
	if(dwTypeOfValue != REG_DWORD)
	{
		if((lResult = RegDeleteValue(hKey,lpWallPaperChangeTimeValueName)) != ERROR_SUCCESS)
		{
			PrintError(lResult,hwnd);
			exit(1);
		}
		VirtualFree(wSettings->lpDirectoryPath,0,MEM_RELEASE);
		InitDefaultSettings(wSettings,IDS_ALLOCATEMEM,&dwLenthOfString);
		WriteSettingsIntoRegistry(wSettings,0,NULL);
		RegCloseKey(hKey);
		return TRUE;
	}
	if((lResult = RegQueryValueEx(hKey,lpWallPaperChangeTimeUnitValueName,NULL,&dwTypeOfValue,(LPBYTE)&(wSettings->dwUnitOfTime),&dwSizeOfData)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		PrintError(lResult,hwnd);
		exit(1);
	}
	if(dwTypeOfValue != REG_DWORD || (dwTypeOfValue != UNIT_SECOND && dwTypeOfValue != UNIT_MINUTE && dwTypeOfValue != UNIT_HOUR && dwTypeOfValue != UNIT_DAYS))
	{
		if((lResult = RegDeleteValue(hKey,lpWallPaperChangeTimeUnitValueName)) != ERROR_SUCCESS)
		{
			PrintError(lResult,hwnd);
			exit(1);
		}
		VirtualFree(wSettings->lpDirectoryPath,0,MEM_RELEASE);
		InitDefaultSettings(wSettings,IDS_ALLOCATEMEM,&dwLenthOfString);
		WriteSettingsIntoRegistry(wSettings,0,NULL);
		RegCloseKey(hKey);
		return TRUE;
	}
	if(wSettings->lpDirectoryPath)
	{
		if((lResult = RegQueryValueEx(hKey,lpWallPaperDirectoryPathValueName,NULL,&dwTypeOfValue,NULL,&dwSizeOfData)) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			PrintError(lResult,hwnd);
			exit(1);
		}
		if(dwTypeOfValue != REG_SZ)
		{
			if(RegDeleteValue(hKey,lpWallPaperDirectoryPathValueName) != ERROR_SUCCESS)
			{
				RegCloseKey(hKey);
				PrintError(GetLastError(),hwnd);
				exit(1);
			}
			VirtualFree(wSettings->lpDirectoryPath,0,MEM_RELEASE);
			InitDefaultSettings(wSettings,IDS_ALLOCATEMEM,&dwLenthOfString);
			WriteSettingsIntoRegistry(wSettings,0,NULL);
			RegCloseKey(hKey);
			return TRUE;
		}
		if(dwSizeOfData / sizeof(TCHAR) > *lpdwLenthOfDirectoryPath)
		{
			RegCloseKey(hKey);
			PrintError(ERROR_INSUFFICIENT_BUFFER,hwnd);
			exit(1);
		}
		if((lResult = RegQueryValueEx(hKey,lpWallPaperDirectoryPathValueName,NULL,&dwTypeOfValue,(LPBYTE)wSettings->lpDirectoryPath,&dwSizeOfData)) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			PrintError(lResult,hwnd);
			exit(1);
		}
	}
	return TRUE;
}
int _wtoiex(TCHAR * StrNumber)
{
	int iErrorNo;
	int iNumber = _wtoi(StrNumber);
	_get_errno(&iErrorNo);
	if(iErrorNo  == ERANGE)
	{
		return 0;
	}
	else
	{
		return iNumber;
	}
}
DWORD MonitorThread(LPVOID lParam)
{
	struct GUIControl * gControl = (struct GUIControl *)lParam;
	HANDLE hAppControlMutex,hAppStartMutex,hAppWallPaperSwapMutex;
	hAppControlMutex = CreateMutex(NULL,FALSE,lpControlMutex);
	hAppStartMutex = CreateMutex(NULL,FALSE,lpAppStartedMutex);
	hAppWallPaperSwapMutex = CreateMutex(NULL,FALSE,lpWallPaperSwapMutex);
	WaitForSingleObject(hDataAccessMutex,INFINITE);
	if(WaitForSingleObject(hAppControlMutex,200) == WAIT_TIMEOUT)
	{
		gControl->SInfo->bControlInstanceStarted = TRUE;
	}
	else
	{
		ReleaseMutex(hAppControlMutex);
		gControl->SInfo->bControlInstanceStarted = FALSE;
	}
	if(WaitForSingleObject(hAppStartMutex,200) == WAIT_TIMEOUT)
	{
		gControl->SInfo->bWallPaperSwapInstanceStarted = TRUE;
	}
	else
	{
		ReleaseMutex(hAppStartMutex);
		gControl->SInfo->bWallPaperSwapInstanceStarted = FALSE;
	}
	if(WaitForSingleObject(hAppWallPaperSwapMutex,200) == WAIT_TIMEOUT)
	{
		gControl->SInfo->bSwappingIsOn = TRUE;
	}
	else
	{
		ReleaseMutex(hAppWallPaperSwapMutex);
		gControl->SInfo->bSwappingIsOn = FALSE;
	}
	CloseHandle(hAppStartMutex);
	CloseHandle(hAppControlMutex);
	CloseHandle(hAppWallPaperSwapMutex);
	return 0;
}
DWORD MonitorWallPaperSwap(__in HWND hEditControl,__in HWND hButtonControl,__out struct StatusInfo * SInfo)
{
	struct GUIControl gControl;
	gControl.hButtonControl = hButtonControl;
	gControl.hEditControl = hEditControl;
	gControl.SInfo = SInfo;
	DWORD dwThreadId;
	HANDLE hMonitorThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)MonitorThread,(LPVOID)&gControl,0,&dwThreadId);
	WaitForSingleObject(hMonitorThread,INFINITE);
	EnableWindow(gControl.hEditControl,FALSE);
	if(gControl.SInfo->bWallPaperSwapInstanceStarted == TRUE && gControl.SInfo->bSwappingIsOn == TRUE)
	{
		SendMessage(gControl.hEditControl,WM_SETTEXT,(WPARAM)0,(LPARAM)TEXT("Running"));
		EnableWindow(gControl.hButtonControl,FALSE);
	}
	else
	{
		SendMessage(gControl.hEditControl,WM_SETTEXT,(WPARAM)0,(LPARAM)TEXT("Not Running"));
		EnableWindow(gControl.hButtonControl,TRUE);
	}
	CloseHandle(hMonitorThread);
	return 0;
}
DWORD RestartSwappingProcess()
{
	HANDLE hSnap = INVALID_HANDLE_VALUE,hProcess = INVALID_HANDLE_VALUE;
	PROCESSENTRY32 pe32;
	LPTSTR lpProcessPath = NULL;
	BOOL bAppFound = FALSE;
	pe32.dwSize = sizeof(PROCESSENTRY32);
	lpProcessPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * 500,MEM_COMMIT,PAGE_READWRITE);
	if(!lpProcessPath)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
	if(hSnap == INVALID_HANDLE_VALUE)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!Process32First(hSnap,&pe32))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	do
	{
		if(!wcsicmp(pe32.szExeFile,lpAppName) && pe32.th32ProcessID != GetCurrentProcessId())
		{
			hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE,FALSE,pe32.th32ProcessID);
			if(hProcess != INVALID_HANDLE_VALUE)
			{
				if(GetModuleFileNameEx(hProcess,NULL,lpProcessPath,500))
				{
					if(!wcsicmp(lpProcessPath,lpAppInstallPath))
					{
						bAppFound = TRUE;
						break;
					}
						
				}
				CloseHandle(hProcess);
				hProcess = INVALID_HANDLE_VALUE;
			}
		}
	}while(Process32Next(hSnap,&pe32));
	if(bAppFound == TRUE)
	{
		TerminateProcess(hProcess,0);
		CloseHandle(hProcess);
		if(!ShellExecute(NULL,TEXT("Open"),lpProcessPath,lpSwapCommand,NULL,NULL))
		{
			PrintError(GetLastError(),NULL);
			exit(1);
		}
	}
	else
	{
		MessageBox(NULL,TEXT("Swap Application is running but cannot be Found. This is because of either the application is at other than standard location or this process have not access to it.Changes will be taken in account by next run."),lpAppName,MB_OK | MB_ICONERROR);
	}
	CloseHandle(hSnap);
	VirtualFree(lpProcessPath,0,MEM_RELEASE);
	return 0;
}
INT_PTR CALLBACK DialogProc(__in  HWND hwndDlg,__in  UINT uMsg,__in  WPARAM wParam,__in  LPARAM lParam)
{
	static WallPaperSettings wSettings = {0};
	static DWORD dwLenthOfString = 0;
	static int iSecondIndex = -1;
	static int iMinuteIndex = -1;
	static int iHourIndex = -1;
	static int iDayIndex = -1;
	static TCHAR NumberString[20];
	static TCHAR DirectoryPath[MAX_PATH];
	static BROWSEINFO bInfo = {0};
	static StatusInfo sInfo = {0};
	static WIN32_FIND_DATA wFind = {0};
	WallPaperSettings wSettingsTemp = {0};
	BOOL bRegularSelection = FALSE;
	int iSelectionIndex = -1;
	int iBufferedIndexValues = -1;
	int iTempIndex  = -1;
	int iTempStringLenth = -1;
	PIDLIST_ABSOLUTE pAbs;
	for(int i = 0 ; i < 20 ; i++)
	{
		NumberString[i] = 0;
	}
	for(int i = 0 ; i < MAX_PATH ; i++)
	{
		DirectoryPath[i] = 0;
	}
	switch(uMsg)
	{
	case WM_INITDIALOG:
		wSettings.lpDirectoryPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
		if(!wSettings.lpDirectoryPath)
		{
			PrintError(GetLastError(),NULL);
			exit(1);
		}
		dwLenthOfString = MAX_PATH;
		ReadCurrentSettingsFromRegistry(&wSettings,&dwLenthOfString,NULL);
		EnableWindow(GetDlgItem(hwndDlg,IDC_EDIT1),FALSE);
		SendMessage(GetDlgItem(hwndDlg,IDC_EDIT1),WM_SETTEXT,(WPARAM)0,(LPARAM)wSettings.lpDirectoryPath);
		iSecondIndex = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_ADDSTRING,(WPARAM)0,(LPARAM)lpSecondUnit);
		iMinuteIndex = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_ADDSTRING,(WPARAM)0,(LPARAM)lpMinuteUnit);
		iHourIndex = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_ADDSTRING,(WPARAM)0,(LPARAM)lpHourUnit);
		iDayIndex = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_ADDSTRING,(WPARAM)0,(LPARAM)lpDayUnit);
		for(int i = 1 ; i <= 60 ; i+=5)
		{
			_itow(i,NumberString,10);
			if(i == 1)
			{
				i = 0;
			}
			if(i == wSettings.dwChangeTime)
			{
				bRegularSelection = TRUE;
			}
			iBufferedIndexValues = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),CB_ADDSTRING,(WPARAM)0,(LPARAM)NumberString);
			if(i == wSettings.dwChangeTime)
			{
				iSelectionIndex = iBufferedIndexValues;
			}
		}
		if(iSelectionIndex != -1)
		{
			SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),CB_SETCURSEL,(WPARAM)iSelectionIndex,(LPARAM)0);
		}
		else
		{
			_itow(wSettings.dwChangeTime,NumberString,10);
			iSelectionIndex = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),CB_ADDSTRING,(WPARAM)0,(LPARAM)NumberString);
			SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),CB_SETCURSEL,(WPARAM)iSelectionIndex,(LPARAM)0);
		}
		switch(wSettings.dwUnitOfTime)
		{
		case UNIT_SECOND:
			SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_SETCURSEL,(WPARAM)iSecondIndex,(LPARAM)0);
			break;
		case UNIT_MINUTE:
			SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_SETCURSEL,(WPARAM)iMinuteIndex,(LPARAM)0);
			break;
        case UNIT_HOUR:
			SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_SETCURSEL,(WPARAM)iHourIndex,(LPARAM)0);
			break;
		case UNIT_DAYS:
			SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_SETCURSEL,(WPARAM)iDayIndex,(LPARAM)0);
			break;
		default:
			PrintError(ERROR_FATAL_APP_EXIT,NULL);
			exit(1);
		}
		MonitorWallPaperSwap(GetDlgItem(hwndDlg,IDC_EDIT2),GetDlgItem(hwndDlg,IDC_BUTTON5),&sInfo);
		EnableWindow(GetDlgItem(hwndDlg,IDC_BUTTON1),FALSE);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_BUTTON3:
			switch(HIWORD(wParam))
			{
			case BN_CLICKED:
				EndDialog(hwndDlg,(INT_PTR)FALSE);
				break;
			}
			break;
		case IDC_BUTTON1:
			switch(HIWORD(wParam))
			{
			case BN_CLICKED:
				iTempIndex = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),CB_GETCURSEL,(WPARAM)0,(LPARAM)0);
				if(iTempIndex != -1)
				{
					iTempStringLenth = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),CB_GETLBTEXTLEN,(WPARAM)iTempIndex,(LPARAM)0);
				}
				else 
				{
					iTempStringLenth = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),WM_GETTEXTLENGTH,(WPARAM)0,(LPARAM)0);
				}
				if(iTempStringLenth > 20)
				{
					MessageBox(hwndDlg,TEXT("Too big String for handle.\n Enter appropriate Number in box."),TEXT("WallPaper Swap"),MB_OK | MB_ICONERROR);
					return (INT_PTR)TRUE;
				}
				if(iTempIndex != -1)
				{
					SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),CB_GETLBTEXT,(WPARAM)iTempIndex,(LPARAM)NumberString);
				}
				else
				{
					SendMessage(GetDlgItem(hwndDlg,IDC_COMBO1),WM_GETTEXT,(WPARAM)MAX_PATH,(LPARAM)NumberString);
				}
				wSettingsTemp.dwChangeTime = _wtoiex(NumberString);
				if(!wSettingsTemp.dwChangeTime || wSettingsTemp.dwChangeTime > 10000)
				{
					MessageBox(hwndDlg,TEXT("Invalid Input Number . \n Enter Appropriate Number in box.(Number should between 1 and 10000."),TEXT("WallPaper Swap"),MB_OK | MB_ICONERROR);
					return (INT_PTR)TRUE;
				}
				iTempIndex = SendMessage(GetDlgItem(hwndDlg,IDC_COMBO2),CB_GETCURSEL,(WPARAM)0,(LPARAM)0);
				if(iTempIndex == iSecondIndex)
				{
					wSettingsTemp.dwUnitOfTime = UNIT_SECOND;
				}
				else if(iTempIndex == iMinuteIndex)
				{
					wSettingsTemp.dwUnitOfTime = UNIT_MINUTE;
				}
				else if(iTempIndex == iHourIndex)
				{
					wSettingsTemp.dwUnitOfTime = UNIT_HOUR;
				}
				else if(iTempIndex == iDayIndex)
				{
					wSettingsTemp.dwUnitOfTime = UNIT_DAYS;
				}
				else
				{
					MessageBox(hwndDlg,TEXT("Invalid Selection made, Please select appropriate Unit."),TEXT("WallPaper Swap"),MB_OK | MB_ICONERROR);
					return (INT_PTR)TRUE;
				}
				iTempStringLenth = SendMessage(GetDlgItem(hwndDlg,IDC_EDIT1),WM_GETTEXTLENGTH,(WPARAM)0,(LPARAM)0);
				if(iTempStringLenth <= 0 || iTempStringLenth > MAX_PATH)
				{
					MessageBox(hwndDlg,TEXT("Invalid or Too big Directory Path , Please select valid Directory Path."),TEXT("WallPaper Swap"),MB_OK | MB_ICONERROR);
					return (INT_PTR)TRUE;
				}
				SendMessage(GetDlgItem(hwndDlg,IDC_EDIT1),WM_GETTEXT,(WPARAM)MAX_PATH,(LPARAM)DirectoryPath);
				wSettingsTemp.lpDirectoryPath = DirectoryPath;
				WriteSettingsIntoRegistry(&wSettingsTemp,0,hwndDlg);
			}
			MonitorWallPaperSwap(NULL,NULL,&sInfo);
			if(sInfo.bSwappingIsOn == TRUE)
			{
				RestartSwappingProcess();
			}
			EnableWindow((HWND)lParam,FALSE);
			break;
		case IDC_BUTTON2:
			switch(HIWORD(wParam))
			{
			case BN_CLICKED:
				SendMessage(hwndDlg,WM_COMMAND,(WPARAM)MAKEWPARAM(IDC_BUTTON1,BN_CLICKED),(LPARAM)GetDlgItem(hwndDlg,IDC_BUTTON1));
				EndDialog(hwndDlg,(INT_PTR)TRUE);
			}
			break;
		case IDC_BUTTON4:
			switch(HIWORD(wParam))
			{
			case BN_CLICKED:
				bInfo.hwndOwner = hwndDlg;
				bInfo.lpszTitle = SZ_INSTRUCT;
				bInfo.pidlRoot = NULL;
				bInfo.ulFlags = BIF_RETURNONLYFSDIRS;
				pAbs = SHBrowseForFolder(&bInfo);
				if(pAbs)
				{
					if(SHGetPathFromIDList(pAbs,DirectoryPath) == TRUE)
					{
						if(FindFirstFileCust(DirectoryPath,&wFind) == INVALID_HANDLE_VALUE)
						{
							MessageBox(hwndDlg,TEXT("Application cannot find any supported Picture File in the Folder specified. Try again or Specify another Directory."),lpAppName,MB_OK | MB_ICONEXCLAMATION);
						}
						else
						{
							SendMessage(GetDlgItem(hwndDlg,IDC_EDIT1),WM_SETTEXT,(WPARAM)0,(LPARAM)DirectoryPath);
							EnableWindow(GetDlgItem(hwndDlg,IDC_BUTTON1),TRUE);
						}
					}
				}
			}
			break;
		case IDC_BUTTON6:
			switch(HIWORD(wParam))
			{
			case BN_CLICKED:
				MonitorWallPaperSwap(GetDlgItem(hwndDlg,IDC_EDIT2),GetDlgItem(hwndDlg,IDC_BUTTON5),&sInfo);
			}
			break;
		case IDC_COMBO1:
			switch(HIWORD(wParam))
			{
			case CBN_SELCHANGE:
				EnableWindow(GetDlgItem(hwndDlg,IDC_BUTTON1),TRUE);
				break;
			case CBN_EDITCHANGE:
				EnableWindow(GetDlgItem(hwndDlg,IDC_BUTTON1),TRUE);
				break;
			}
			break;
		case IDC_BUTTON5:
			ShellExecute(NULL,TEXT("Open"),lpAppInstallPath,lpSwapCommand,NULL,SW_SHOWDEFAULT);
			break;
		case IDC_COMBO2:
			switch(HIWORD(wParam))
			{
			case CBN_SELCHANGE:
				EnableWindow(GetDlgItem(hwndDlg,IDC_BUTTON1),TRUE);
				break;
			}
			break;
		}
		break;
		case WM_CLOSE:
			EndDialog(hwndDlg,(INT_PTR)TRUE);
		break;
		
			
	}
	return (INT_PTR)FALSE;
}
BOOL InstallApplication()
{
	HRESULT hRes;
	IShellLink * pIsl = NULL;
	IPersistFile * pIPersistFile = NULL;
	DWORD dwPathSize = 0;
	LPTSTR lpApplicationPath = NULL;
	LPTSTR lpDescription = NULL;
	LPTSTR lpArgument = NULL;
	LPTSTR lpAppName = NULL;
	LPTSTR lpThisModuleFileName = NULL;
	LPTSTR lpDesktopPath = NULL;
	LPTSTR lpRegKey = NULL;
	LPTSTR lpRunCommand = NULL;
	DWORD dwSizeOfExtraSpace = 80;
	HKEY hKey,hRunKey;
	DWORD dwLenthOfDirectoryPath = 0;
	WallPaperSettings wSettingsDefault = {0};
	lpRegKey = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!lpRegKey)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_REGKEY,lpRegKey,MAX_PATH))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	lpDescription = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!lpDescription)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	lpArgument = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!lpArgument)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	hRes = CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(LPVOID *)&pIsl);
	if(FAILED(hRes))
	{
		PrintError(HRESULT_CODE(hRes),NULL);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_LINKDESC,lpDescription,MAX_PATH))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_LINKARGUMENT,lpArgument,MAX_PATH))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	dwPathSize = ExpandEnvironmentStrings(TEXT("%APPDATA%"),lpApplicationPath,0);
	lpApplicationPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (dwPathSize + dwSizeOfExtraSpace),MEM_COMMIT,PAGE_READWRITE);
	if(!lpApplicationPath)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!ExpandEnvironmentStrings(TEXT("%APPDATA%"),lpApplicationPath,dwPathSize + dwSizeOfExtraSpace))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	lpAppName = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
	if(!lpAppName)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_APPNAME,lpAppName,MAX_PATH))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	wcscat_s(lpApplicationPath,dwPathSize + dwSizeOfExtraSpace,TEXT("\\"));
	wcscat_s(lpApplicationPath,dwPathSize + dwSizeOfExtraSpace,lpAppName);
	CreateDirectory(lpApplicationPath,NULL);
	wcscat_s(lpApplicationPath,dwPathSize + dwSizeOfExtraSpace,TEXT("\\"));
	wcscat_s(lpApplicationPath,dwPathSize + dwSizeOfExtraSpace,lpAppName);
	wcscat_s(lpApplicationPath,dwPathSize + dwSizeOfExtraSpace,TEXT(".exe"));
	if(!lpAppInstallPath)
	{
		lpAppInstallPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (wcslen(lpApplicationPath) + 1),MEM_COMMIT,PAGE_READWRITE);
		if(!lpAppInstallPath)
		{
			PrintError(GetLastError(),NULL);
			exit(1);
		}
		wcscpy_s(lpAppInstallPath,wcslen(lpApplicationPath) + 1,lpApplicationPath);
	}
	if(GetThisModuleFileName(&lpThisModuleFileName) == FALSE)
	{
		PrintError(5,NULL);
		exit(1);
	}
	if(!CopyFile(lpThisModuleFileName,lpApplicationPath,FALSE))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	hRes = pIsl->SetDescription(lpDescription);
	if(FAILED(hRes))
	{
		PrintError(HRESULT_CODE(hRes),NULL);
		exit(1);
	}
	hRes = pIsl->SetArguments(lpArgument);
	if(FAILED(hRes))
	{
		PrintError(HRESULT_CODE(hRes),NULL);
		exit(1);
	}
	hRes = pIsl->SetPath(lpApplicationPath);
	if(FAILED(hRes))
	{
		PrintError(HRESULT_CODE(hRes),NULL);
		exit(1);
	}
	hRes = pIsl->QueryInterface(IID_IPersistFile,(void **)&pIPersistFile);
	if(FAILED(hRes))
	{
		PrintError(HRESULT_CODE(hRes),NULL);
		exit(1);
	}
	lpDesktopPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (MAX_PATH + dwSizeOfExtraSpace),MEM_COMMIT,PAGE_READWRITE);
	if(!lpDesktopPath)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	SHGetFolderPath(NULL,CSIDL_DESKTOPDIRECTORY,NULL,SHGFP_TYPE_CURRENT,lpDesktopPath);
	wcscat_s(lpDesktopPath,MAX_PATH + dwSizeOfExtraSpace,TEXT("\\"));
	wcscat_s(lpDesktopPath,MAX_PATH + dwSizeOfExtraSpace,lpAppName);
	wcscat_s(lpDesktopPath,MAX_PATH + dwSizeOfExtraSpace,TEXT(".lnk"));
	hRes = pIPersistFile->Save(lpDesktopPath,FALSE);
	if(FAILED(hRes))
	{
		PrintError(HRESULT_CODE(hRes),NULL);
		exit(1);
	}
	pIsl->Release();
	pIPersistFile->Release();
	if(RegCreateKeyEx(HKEY_CURRENT_USER,lpRegKey,0,NULL,NULL,KEY_READ,NULL,&hKey,NULL) != ERROR_SUCCESS)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(RegCreateKeyEx(HKEY_CURRENT_USER,lpRunKey,0,NULL,NULL,KEY_SET_VALUE,NULL,&hRunKey,NULL) != ERROR_SUCCESS)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	lpRunCommand = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (wcslen(lpAppInstallPath) + wcslen(lpSwapCommand) + 4),MEM_COMMIT,PAGE_READWRITE);
	if(!lpRunCommand)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	wcscpy_s(lpRunCommand,wcslen(lpAppInstallPath) + wcslen(lpSwapCommand) + 4,TEXT("\""));
	wcscat_s(lpRunCommand,wcslen(lpAppInstallPath) + wcslen(lpSwapCommand) + 4,lpAppInstallPath);
	wcscat_s(lpRunCommand,wcslen(lpAppInstallPath) + wcslen(lpSwapCommand) + 4,TEXT("\" "));
	wcscat_s(lpRunCommand,wcslen(lpAppInstallPath) + wcslen(lpSwapCommand) + 4,lpSwapCommand);
	if(RegSetValueEx(hRunKey,lpRunValueName,0,REG_SZ,(const BYTE *)lpRunCommand,(wcslen(lpAppInstallPath) + wcslen(lpSwapCommand) + 4) * sizeof(TCHAR)) != ERROR_SUCCESS)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	RegCloseKey(hKey);
	RegCloseKey(hRunKey);
	VirtualFree(lpRunCommand,0,MEM_RELEASE);
	VirtualFree(lpApplicationPath,0,MEM_RELEASE);
	VirtualFree(lpDescription,0,MEM_RELEASE);
	VirtualFree(lpDesktopPath,0,MEM_RELEASE);
	VirtualFree(lpAppName,0,MEM_RELEASE);
	VirtualFree(lpArgument,0,MEM_RELEASE);
	VirtualFree(lpRegKey,0,MEM_RELEASE);
	InitDefaultSettings(&wSettingsDefault,IDS_ALLOCATEMEM,&dwLenthOfDirectoryPath);
	WriteSettingsIntoRegistry(&wSettingsDefault,0,NULL);
	return TRUE;
}
HANDLE CreateAppStartMutex()
{
	HANDLE hAppStartMutex = CreateMutex(NULL,TRUE,lpAppStartedMutex);
	return hAppStartMutex;
}
HANDLE CreateControlMutex()
{
	HANDLE hControlMutex = CreateMutex(NULL,TRUE,lpControlMutex);
	return hControlMutex;
}
HANDLE CreateSwappingMutex()
{
	HANDLE hSwapMutex = CreateMutex(NULL,TRUE,lpWallPaperSwapMutex);
	return hSwapMutex;
}
void Setwallpaper(TCHAR *lpWallPaperPath)
{
	if(!lpWallPaperPath)
	{
		return;
	}
	HRESULT hr;
	HRESULT hSecond;
	IActiveDesktop *pActiveDesktop;

	hr = CoCreateInstance(CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER,
					  IID_IActiveDesktop, (void**)&pActiveDesktop);
	if(!SUCCEEDED(hr))
	{
		PrintError(HRESULT_CODE(hr),NULL);
		exit(1);
	}
	hSecond=pActiveDesktop->SetWallpaper(lpWallPaperPath,0);
	if(!SUCCEEDED(hSecond))
	{
		PrintError(HRESULT_CODE(hSecond),NULL);
		exit(1);
	}
    pActiveDesktop->ApplyChanges(AD_APPLY_ALL);
	pActiveDesktop->Release();
}
DWORD GetFileExtension(__in LPTSTR lpFileName,__out LPTSTR lpExtenstion,__in DWORD dwBufferLenth,__inout LPDWORD lpdwReturnLenth)
{
	if(!lpFileName || !lpExtenstion || dwBufferLenth < 3)
	{
		return 0;
	}
	DWORD dwLenthOfExtension = 0;
	DWORD dwExtensionIndex = 0;
	for(int i = wcslen(lpFileName) - 1 ; i >= 0 ; i--)
	{
		if(lpFileName[i] == L'.')
		{
			dwExtensionIndex = i;
			break;
		}
		dwLenthOfExtension++;
	}
	if(dwLenthOfExtension == wcslen(lpFileName) || dwLenthOfExtension == 0)
	{
		lpExtenstion[0] = 0;
		if(lpdwReturnLenth)
		{
			*lpdwReturnLenth = 0;
		}
		return 1;
	}
	else if(dwLenthOfExtension >= dwBufferLenth)
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		if(lpdwReturnLenth)
		{
			*lpdwReturnLenth = dwLenthOfExtension + 1;
		}
		return 0;
	}
	else
	{
		if(lpdwReturnLenth)
		{
			*lpdwReturnLenth = dwLenthOfExtension + 1;
		}
		memcpy(lpExtenstion ,lpFileName + (wcslen(lpFileName) - dwLenthOfExtension),dwLenthOfExtension * sizeof(TCHAR));
		return 1;
	}
}
HANDLE FindFirstFileCust(__in LPCTSTR lpFolderPath,__out LPWIN32_FIND_DATA lpFindData)
{
	if(!lpFolderPath || !lpFindData)
	{
		return INVALID_HANDLE_VALUE;
	}
	HANDLE hFindFileHandle;
	TCHAR lpFileExtension[10] = {0};
	BOOL bFileFound = FALSE;
	DWORD dwExtraSpaceNeeded = (lpFolderPath[wcslen(lpFolderPath) - 1] == '\\' ? 4 : 5);
	LPTSTR lpSearchString = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (wcslen(lpFolderPath) + dwExtraSpaceNeeded),MEM_COMMIT,PAGE_READWRITE);
	if(!lpSearchString)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	wcscpy_s(lpSearchString,wcslen(lpFolderPath) + dwExtraSpaceNeeded,lpFolderPath);
	if(dwExtraSpaceNeeded > 4)
	{
		wcscat_s(lpSearchString,wcslen(lpFolderPath) + dwExtraSpaceNeeded,TEXT("\\*"));
	}
	else
	{
		wcscat_s(lpSearchString,wcslen(lpFolderPath) + dwExtraSpaceNeeded,TEXT("*"));
	}
	hFindFileHandle = FindFirstFile(lpSearchString,lpFindData);
	if(hFindFileHandle == INVALID_HANDLE_VALUE)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	do
	{
		if((!(lpFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) && (!(lpFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) && (!(lpFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)))
		{
			if(GetFileExtension(lpFindData->cFileName,lpFileExtension,10,NULL))
			{
				for(int i = 0 ; lpSupportedExtensionList[i] && !bFileFound ; i++)
				{
					if(!wcsicmp(lpSupportedExtensionList[i],lpFileExtension))
					{
						bFileFound = TRUE;
					}
				}
			}
			
		}
		
		
	}while(!bFileFound && FindNextFile(hFindFileHandle,lpFindData));
	VirtualFree(lpSearchString,0,MEM_RELEASE);
	if(bFileFound == FALSE)
	{
		FindClose(hFindFileHandle);
		return INVALID_HANDLE_VALUE;
	}
	else
	{
		return hFindFileHandle;
	}
}

DWORD WaitForTimeInterval(__in DWORD64 dwTimeInterval)
{
	DWORD dwTimeInterval32;
	DWORD64 dwTimeIntervalTemp = dwTimeInterval;
	LPTSTR lpRegSubKey = NULL;
	HKEY hKey;
	LRESULT lResult;
	if(!dwTimeInterval)
	{
		return 0;
	}
	if(dwTimeInterval <= TIMER_THRESHOLD)
	{
		dwTimeInterval32 = (DWORD) dwTimeInterval;
		Sleep(dwTimeInterval32);
	}
	else
	{
		dwTimeInterval32 = TIMER_THRESHOLD;
		DWORD dwTypeOfRegValue;
		DWORD dwSizeOfData = sizeof(DWORD64);
		lpRegSubKey = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * MAX_PATH,MEM_COMMIT,PAGE_READWRITE);
		if(!lpRegSubKey)
		{
			PrintError(GetLastError(),NULL);
			exit(1);
		}
		if(!LoadString(GetThisModuleInstance(NULL),IDS_REGKEY,lpRegSubKey,MAX_PATH))
		{
			PrintError(GetLastError(),NULL);
			exit(1);
		}
		if((lResult = RegOpenKeyEx(HKEY_CURRENT_USER,lpRegSubKey,0,KEY_SET_VALUE | KEY_READ,&hKey)) != ERROR_SUCCESS)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
		if((lResult = RegQueryValueEx(hKey,lpTempTimeHolderValueName,0,&dwTypeOfRegValue,(LPBYTE)&dwTimeIntervalTemp,&dwSizeOfData)) != ERROR_SUCCESS)
		{
			if(lResult == ERROR_FILE_NOT_FOUND)
			{
				if((lResult = RegSetValueEx(hKey,lpTempTimeHolderValueName,0,REG_BINARY,(const BYTE *)&dwTimeIntervalTemp,sizeof(DWORD64))) != ERROR_SUCCESS)
				{
					PrintError(lResult,NULL);
					exit(1);
				}
			}
			else 
			{
				PrintError(lResult,NULL);
				exit(1);
			}
		}
		else if(dwTypeOfRegValue != REG_BINARY || dwSizeOfData > sizeof(DWORD64))
		{
			if((lResult = RegDeleteValue(hKey,lpTempTimeHolderValueName)) != ERROR_SUCCESS)
			{
				PrintError(lResult,NULL);
				exit(1);
			}
			if((lResult = RegSetValueEx(hKey,lpTempTimeHolderValueName,0,REG_BINARY,(const BYTE *)&dwTimeIntervalTemp,sizeof(DWORD64))) != ERROR_SUCCESS)
			{
				PrintError(lResult,NULL);
				exit(1);
			}
		}
		if((lResult = RegQueryValueEx(hKey,lpTempTimeHolderValueName,0,&dwTypeOfRegValue,(LPBYTE)&dwTimeIntervalTemp,&dwSizeOfData)) != ERROR_SUCCESS)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
		while(dwTimeIntervalTemp > dwTimeInterval32)
		{
			Sleep(dwTimeInterval32);
			dwTimeIntervalTemp -= (DWORD64)dwTimeInterval32;
			if((lResult = RegSetValueEx(hKey,lpTempTimeHolderValueName,0,REG_BINARY,(const BYTE *)&dwTimeIntervalTemp,sizeof(DWORD64))) != ERROR_SUCCESS)
			{
				PrintError(lResult,NULL);
				exit(1);
			}
		}
	    dwTimeInterval32 = (DWORD)dwTimeIntervalTemp;
		Sleep(dwTimeInterval32);
		if((lResult = RegDeleteValue(hKey,lpTempTimeHolderValueName)) != ERROR_SUCCESS)
		{
			PrintError(GetLastError(),NULL);
			exit(1);
		}
		RegCloseKey(hKey);
		VirtualFree(lpRegSubKey,0,MEM_RELEASE);
	}
    return 0;
}

BOOL FindNextFileCust(__in HANDLE hFindFileHandle,__out LPWIN32_FIND_DATA lpFindData)
{
	BOOL bFileFound = FALSE;
	TCHAR lpFileExtension[10] = {0};
	while(!bFileFound)
	{
		if(!FindNextFile(hFindFileHandle,lpFindData))
		{
			break;
		}
		if((!(lpFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) && (!(lpFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) && (!(lpFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)))
		{
			if(GetFileExtension(lpFindData->cFileName,lpFileExtension,10,NULL))
			{
				for(int i = 0 ; lpSupportedExtensionList[i] && !bFileFound ; i++)
				{
					if(!wcsicmp(lpSupportedExtensionList[i],lpFileExtension))
					{
						bFileFound = TRUE;
					}
				}
			}
			
		}
	}
	return bFileFound;
}
BOOL FindCloseFileCust(__in HANDLE hFindFileHandle)
{
	return FindClose(hFindFileHandle);
}
DWORD SwapWallPaper()
{
	DWORD64 dwTimeValue;
	HKEY hKey;
	LRESULT lResult;
	TCHAR lpRegKey[50] = {0};
	TCHAR CurrentFileName[260] = {0};
	WallPaperSettings wSettings = {0};
	TCHAR WallPaperPath[500] = {0};
	DWORD dwIndexOfLastSlash = 0;
	WIN32_FIND_DATA wFindData;
	DWORD dwLenthOfDirectoryPath = MAX_PATH;
	DWORD dwSizeOfBuffer = 260 * sizeof(TCHAR);
	DWORD dwTypeOfRegValue = 0;
	BOOL bResumeFromPreviousFile = FALSE;
	if(!LoadString(GetThisModuleInstance(NULL),IDS_REGKEY,lpRegKey,50))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if((lResult = RegOpenKeyEx(HKEY_CURRENT_USER,lpRegKey,0,KEY_SET_VALUE | KEY_READ,&hKey)) != ERROR_SUCCESS)
	{
		PrintError(lResult,NULL);
		exit(1);
	}
	if((lResult = RegQueryValueEx(hKey,lpCurrentWallPaperFileName,NULL,&dwTypeOfRegValue,(LPBYTE)CurrentFileName,&dwSizeOfBuffer)) != ERROR_SUCCESS)
	{
		if(lResult == ERROR_FILE_NOT_FOUND  || (lResult == ERROR_MORE_DATA && dwTypeOfRegValue != REG_SZ))
		{
		}
		else
		{
			PrintError(lResult,NULL);
			exit(1);
		}
	}
	else if(dwTypeOfRegValue != REG_SZ)
	{
		if((lResult = RegDeleteValue(hKey,lpCurrentWallPaperFileName)) != ERROR_SUCCESS)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
	}
	else
	{
		bResumeFromPreviousFile = TRUE;
	}
	wSettings.lpDirectoryPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * dwLenthOfDirectoryPath,MEM_COMMIT,PAGE_READWRITE);
	ReadCurrentSettingsFromRegistry(&wSettings,&dwLenthOfDirectoryPath,NULL);
	switch(wSettings.dwUnitOfTime)
	{
	case UNIT_DAYS:
		dwTimeValue = wSettings.dwChangeTime * 60 * 60 * 24 * 1000;
		break;
	case UNIT_MINUTE:
		dwTimeValue = wSettings.dwChangeTime * 60 * 1000;
		break;
	case UNIT_HOUR:
		dwTimeValue = wSettings.dwChangeTime * 60 * 60 * 1000;
		break;
	case UNIT_SECOND:
		dwTimeValue = wSettings.dwChangeTime * 1000;
	}
	wcscpy_s(WallPaperPath,500,wSettings.lpDirectoryPath);
	wcscat_s(WallPaperPath,500,TEXT("\\"));
	dwIndexOfLastSlash = wcslen(WallPaperPath) - 1;
	HANDLE hFindFileHandle = FindFirstFileCust(wSettings.lpDirectoryPath,&wFindData);
	if(hFindFileHandle == INVALID_HANDLE_VALUE)
	{
		PrintError(ERROR_FILE_NOT_FOUND,NULL);
		exit(1);
	}
	if(bResumeFromPreviousFile == FALSE)
	{
		wcscpy_s(CurrentFileName,260,wFindData.cFileName);
	}
	else
	{
		BOOL bFileFound = FALSE;
		do
		{
			if(!wcsicmp(wFindData.cFileName,CurrentFileName))
			{
				bFileFound = TRUE;
				break;
			}
		}while(FindNextFileCust(hFindFileHandle,&wFindData));
		if(bFileFound == FALSE)
		{
			FindCloseFileCust(hFindFileHandle);
			hFindFileHandle = FindFirstFileCust(wSettings.lpDirectoryPath,&wFindData);
		}
	    wcscpy_s(CurrentFileName,260,wFindData.cFileName);
			
	}
	do
	{
		wcscpy_s(CurrentFileName,260,wFindData.cFileName);
		if((lResult = RegSetValueEx(hKey,lpCurrentWallPaperFileName,0,REG_SZ,(const BYTE *)CurrentFileName,(wcslen(CurrentFileName) + 1) * sizeof(TCHAR))) != ERROR_SUCCESS)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
		WallPaperPath[dwIndexOfLastSlash + 1] = 0;
		wcscat_s(WallPaperPath,500,CurrentFileName);
	    WaitForTimeInterval(dwTimeValue);
		Setwallpaper(WallPaperPath);
	}while(FindNextFileCust(hFindFileHandle,&wFindData));
	FindCloseFileCust(hFindFileHandle);
	if((lResult = RegDeleteValue(hKey,lpCurrentWallPaperFileName)) != ERROR_SUCCESS)
	{
		PrintError(lResult,NULL);
		exit(1);
	}
	return 0;
}

DWORD UninstallApplication()
{
	LSTATUS lResult = ERROR_SUCCESS;
	HKEY hKey = NULL;
	DWORD dwSizeOfExtraSpace = 20;
	LPTSTR lpRegKey = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * 50,MEM_COMMIT,PAGE_READWRITE);
	LPTSTR lpDesktopPath = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * (MAX_PATH + dwSizeOfExtraSpace),MEM_COMMIT,PAGE_READWRITE);
	LPTSTR lpAppName = (LPTSTR) VirtualAlloc(NULL,sizeof(TCHAR) * 50,MEM_COMMIT,PAGE_READWRITE);
	if(!lpRegKey)
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_REGKEY,lpRegKey,50))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if(!LoadString(GetThisModuleInstance(NULL),IDS_APPNAME,lpAppName,50))
	{
		PrintError(GetLastError(),NULL);
		exit(1);
	}
	if((lResult = RegDeleteKey(HKEY_CURRENT_USER,lpRegKey)) != ERROR_SUCCESS)
	{
		if(lResult != ERROR_NOT_FOUND && lResult != ERROR_FILE_NOT_FOUND)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
	}
	for(int j = wcslen(lpRegKey) - 1; j >= 0 ; j--)
	{
		if(lpRegKey[j] == L'\\')
		{
			lpRegKey[j] = L'\0';
			break;
		}
	}
	if((lResult = RegDeleteKey(HKEY_CURRENT_USER,lpRegKey)) != ERROR_SUCCESS)
	{
		if(lResult != ERROR_NOT_FOUND && lResult != ERROR_FILE_NOT_FOUND)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
	}
	if((lResult = RegOpenKeyEx(HKEY_CURRENT_USER,lpRunKey,0,KEY_SET_VALUE,&hKey)) != ERROR_SUCCESS)
	{
		if(lResult != ERROR_NOT_FOUND && lResult != ERROR_FILE_NOT_FOUND)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
	}
	if((lResult = RegDeleteValue(hKey,lpRunValueName)) != ERROR_SUCCESS)
	{
		if(lResult != ERROR_NOT_FOUND && lResult != ERROR_FILE_NOT_FOUND)
		{
			PrintError(lResult,NULL);
			exit(1);
		}
	}
	
	SHGetFolderPath(NULL,CSIDL_DESKTOPDIRECTORY,NULL,SHGFP_TYPE_CURRENT,lpDesktopPath);
	wcscat_s(lpDesktopPath,MAX_PATH + dwSizeOfExtraSpace,TEXT("\\"));
	wcscat_s(lpDesktopPath,MAX_PATH + dwSizeOfExtraSpace,lpAppName);
	wcscat_s(lpDesktopPath,MAX_PATH + dwSizeOfExtraSpace,TEXT(".lnk"));
	DeleteFile(lpDesktopPath);
	MessageBox(NULL,TEXT("Some part of the Application cannot be removed because it is in use. To remove that part manually go to application data folder and delete the folder named \"WallPaper Swap\"."),lpAppName,MB_OK | MB_ICONINFORMATION);
	return 0;
}
int _stdcall wWinMain(HINSTANCE hInstance,HINSTANCE hPrivInstance,LPTSTR lpCmdLine,int nCmdShow)
{
	HANDLE hAppStartMutex = INVALID_HANDLE_VALUE,hAppControlMutex = INVALID_HANDLE_VALUE,hSwapMutex = INVALID_HANDLE_VALUE;
	DWORD dwResult;
	TCHAR lpAppName[30] = {0};
	hAppStartMutex = CreateAppStartMutex();
	WaitForSingleObject(hAppStartMutex,200);
	InitApp(hInstance);
	if(IsThisFirstTime())
	{
		if(!wcsicmp(lpCmdLine,lpUninstallCommand))
		{
			if(MessageBox(NULL,TEXT("Application is either not installed or it is already uninstalled before. Are you sure to run uninstall Procedure again ??"),TEXT("WallPaperSwap"),MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
			{
				exit(1);
			}
		}
		else
		{
			dwResult = MessageBox(NULL,TEXT("WallPaperSwap Application is not installed. Would you like to install it now ??"),TEXT("WallPaperSwap"),MB_OKCANCEL | MB_ICONQUESTION);
			if(dwResult == IDCANCEL)
			{
				return 0;
			}
			InstallApplication();
			MessageBox(NULL,TEXT("WallPaperSwap Application has been successfully installed."),TEXT("WallPaperSwap"),MB_OK | MB_ICONINFORMATION);
			if(!lpCmdLine[0])
			{
				exit(1);
			}
		}
	}
	if(!LoadString(hInstance,IDS_APPNAME,lpAppName,30))
	{
		PrintError(GetLastError(),NULL);
		return 0;
	}
	//Control Instance
	if(!wcsicmp(lpCmdLine,lpControlCommand))
	{
		hAppControlMutex = CreateControlMutex();
		if(WaitForSingleObject(hAppControlMutex,200) != WAIT_OBJECT_0)
		{
			MessageBox(NULL,TEXT("One instance of WallPaper Control is already running. Close that instance and try again."),lpAppName,MB_OK | MB_ICONERROR);
			exit(1);
		}
		DialogBox(hInstance,MAKEINTRESOURCE(IDD_DIALOG1),NULL,DialogProc);
	}
    else if(!wcsicmp(lpCmdLine,lpSwapCommand))
	{
		hSwapMutex = CreateSwappingMutex();
		if(WaitForSingleObject(hSwapMutex,200) != WAIT_OBJECT_0)
		{
			MessageBox(NULL,TEXT("One instance of WallPaper Swap is already running. Close that instance and try again."),lpAppName,MB_OK | MB_ICONERROR);
			exit(1);
		}
		for(;;)
		{
			SwapWallPaper();
		}
	}
	else if(!wcsicmp(lpCmdLine,lpUninstallCommand))
	{
		DWORD dwFeedBack = MessageBox(NULL,TEXT("Are you sure you want to uninstall the Application ??"),lpAppName,MB_OKCANCEL | MB_ICONINFORMATION);
		if(dwFeedBack == IDOK)
		{
			UninstallApplication();
		}
	}
	else
	{
		MessageBox(NULL,TEXT("Usage :=  /Swap := Swap the wallPaper and /Control := Open up wallpaper control Panel and /Uninstall := uninstall the application."),TEXT("WallPaper Swap"),MB_OK | MB_ICONINFORMATION);
	}
	return 0;
}
 