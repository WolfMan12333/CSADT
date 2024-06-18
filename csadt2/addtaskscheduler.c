#include <stdio.h>
#include <windows.h>
#include <taskschd.h>
#include <combaseapi.h>
#include "addtaskscheduler.h"
#include "beacon.h"

HRESULT SetDailyTask(HRESULT hr, ITriggerCollection* pTriggerCollection, wchar_t* startTime, wchar_t* expireTime, int daysInterval, wchar_t* delay) {
	IID IIDIDailyTrigger = {0x126c5cd8, 0xb288, 0x41d5, {0x8d, 0xbf, 0xe4, 0x91, 0x44, 0x6a, 0xdc, 0x5c}};
	ITrigger* pTrigger = NULL;
	
	hr = pTriggerCollection->lpVtbl->Create(pTriggerCollection, TASK_TRIGGER_DAILY, &pTrigger);
	if (SUCCEEDED(hr)) {
		IDailyTrigger* pDailyTrigger = NULL;
		
		hr = pTrigger->lpVtbl->QueryInterface(pTrigger, &IIDIDailyTrigger, (void**)&pDailyTrigger);
		if (SUCCEEDED(hr)) {
			BSTR startTimeBstr = OLEAUT32$SysAllocString(startTime);
			BSTR expireTimeBstr = OLEAUT32$SysAllocString(expireTime);
			BSTR delayBstr = OLEAUT32$SysAllocString(delay);

			pDailyTrigger->lpVtbl->put_StartBoundary(pDailyTrigger, startTimeBstr); 
			pDailyTrigger->lpVtbl->put_EndBoundary(pDailyTrigger, expireTimeBstr); 
			pDailyTrigger->lpVtbl->put_DaysInterval(pDailyTrigger, daysInterval); 
			pDailyTrigger->lpVtbl->put_RandomDelay(pDailyTrigger, delayBstr); 
			pDailyTrigger->lpVtbl->Release(pDailyTrigger);
			
			OLEAUT32$SysFreeString(startTimeBstr);
			OLEAUT32$SysFreeString(expireTimeBstr);
			OLEAUT32$SysFreeString(delayBstr);
		}
		pTrigger->lpVtbl->Release(pTrigger);
	}
	pTriggerCollection->lpVtbl->Release(pTriggerCollection);
	
	return hr;
}

BOOL IsElevated() {
    BOOL fIsElevated = FALSE;
    HANDLE hToken = NULL;

    if (ADVAPI32$OpenProcessToken(KERNEL32$GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize;

        if (ADVAPI32$GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            fIsElevated = elevation.TokenIsElevated;
        }
    }

    if (hToken) {
        KERNEL32$CloseHandle(hToken);
    }
    return fIsElevated;
}


BOOL CreateScheduledTask(char* triggerType, wchar_t* taskName, wchar_t * host, wchar_t* programPath, wchar_t* programArguments, wchar_t* startTime, wchar_t* expireTime, int daysInterval, wchar_t* delay, wchar_t* userID, wchar_t* repeatTask) {
    BOOL actionResult = FALSE;
	HRESULT hr = S_OK;

    hr = OLE32$CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return actionResult;

	IID CTaskScheduler = {0x0f87369f, 0xa4e5, 0x4cfc, {0xbd,0x3e,0x73,0xe6,0x15,0x45,0x72,0xdd}};
	IID IIDITaskService = {0x2faba4c7, 0x4da9, 0x4013, {0x96, 0x97, 0x20, 0xcc, 0x3f, 0xd4, 0x0f, 0x85}};
	ITaskService *pTaskService = NULL;
    hr = OLE32$CoCreateInstance(&CTaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IIDITaskService, (void**)&pTaskService);
    if (FAILED(hr)) {
        return actionResult;
    }
	
	VARIANT Vhost;
	VARIANT VNull;
	OLEAUT32$VariantInit(&Vhost);
	OLEAUT32$VariantInit(&VNull);
	Vhost.vt = VT_BSTR;
	Vhost.bstrVal = OLEAUT32$SysAllocString(host);
	
	hr = pTaskService->lpVtbl->Connect(pTaskService, Vhost, VNull, VNull, VNull); 
    if (FAILED(hr)) {
		goto cleanup;
    }
	
	ITaskFolder* pTaskFolder = NULL;
	BSTR folderPathBstr = OLEAUT32$SysAllocString(L"\\");
	hr = pTaskService->lpVtbl->GetFolder(pTaskService, folderPathBstr, &pTaskFolder);
	if (FAILED(hr)) {
		goto cleanup;
	}
	OLEAUT32$SysFreeString(folderPathBstr);

    ITaskDefinition* pTaskDefinition = NULL;
    hr = pTaskService->lpVtbl->NewTask(pTaskService, 0, &pTaskDefinition);
    if (FAILED(hr)) {
		goto cleanup;
    }
	
	BOOL isRemoteHost = (Vhost.bstrVal && *Vhost.bstrVal);
	IPrincipal* pPrincipal = NULL;
	hr = pTaskDefinition->lpVtbl->get_Principal(pTaskDefinition, &pPrincipal);
	if (SUCCEEDED(hr)) {
		if (IsElevated() || isRemoteHost) {
			BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Running in elevated context and setting \"Run whether user is logged on or not\" security option as SYSTEM!\n"); 
			BSTR systemUser = OLEAUT32$SysAllocString(L"SYSTEM");
			pPrincipal->lpVtbl->put_UserId(pPrincipal, systemUser);
			OLEAUT32$SysFreeString(systemUser);
		}else {
			pPrincipal->lpVtbl->put_LogonType(pPrincipal, TASK_LOGON_INTERACTIVE_TOKEN);
		}
		pPrincipal->lpVtbl->Release(pPrincipal);
	}
	

    ITriggerCollection* pTriggerCollection = NULL;
    hr = pTaskDefinition->lpVtbl->get_Triggers(pTaskDefinition, &pTriggerCollection);
    if (FAILED(hr)) {
		goto cleanup;
    }

	//trigger options
	if (MSVCRT$strcmp(triggerType, "daily") == 0) {
		hr = SetDailyTask(hr, pTriggerCollection, startTime, expireTime, daysInterval, delay); 
	} else {
		goto cleanup;
	}
	
	IActionCollection* pActionCollection = NULL;
    hr = pTaskDefinition->lpVtbl->get_Actions(pTaskDefinition, &pActionCollection);
    if (FAILED(hr)) {
		goto cleanup;
    }
	
    IAction* pAction = NULL;
    hr = pActionCollection->lpVtbl->Create(pActionCollection, TASK_ACTION_EXEC, &pAction);
    if (FAILED(hr)) {
		goto cleanup;
    }

	IID IIDIExecAction = {0x4c3d624d, 0xfd6b, 0x49a3, {0xb9, 0xb7, 0x09, 0xcb, 0x3c, 0xd3, 0xf0, 0x47}};
    IExecAction* pExecAction = NULL;
    hr = pAction->lpVtbl->QueryInterface(pAction, &IIDIExecAction, (void**)&pExecAction);
    if (FAILED(hr)) {
		goto cleanup;
    }
	
	BSTR programPathBstr = OLEAUT32$SysAllocString(programPath);
    hr = pExecAction->lpVtbl->put_Path(pExecAction, programPathBstr);
    if (FAILED(hr)) {
		goto cleanup;
    }
	OLEAUT32$SysFreeString(programPathBstr);
	
	BSTR programArgumentsBstr = OLEAUT32$SysAllocString(programArguments);
    hr = pExecAction->lpVtbl->put_Arguments(pExecAction, programArgumentsBstr);
    if (FAILED(hr)) {
		goto cleanup;
    }
	OLEAUT32$SysFreeString(programArgumentsBstr);
	
    pExecAction->lpVtbl->Release(pExecAction);
    pAction->lpVtbl->Release(pAction);
	
    IRegisteredTask* pRegisteredTask = NULL;
	hr = pTaskFolder->lpVtbl->RegisterTaskDefinition(pTaskFolder, taskName, pTaskDefinition, TASK_CREATE_OR_UPDATE, VNull, VNull, TASK_LOGON_INTERACTIVE_TOKEN, VNull, &pRegisteredTask);

	if (FAILED(hr)) {
        BeaconPrintf(CALLBACK_ERROR, "csb BOF: Failed to register the scheduled task with error code: %x\n", hr);
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Scheduled task '%ls' created successfully!\n", taskName);
        actionResult = TRUE;
    }
	
cleanup:
    if (pRegisteredTask) {
        pRegisteredTask->lpVtbl->Release(pRegisteredTask);
    }
    if (pActionCollection) {
        pActionCollection->lpVtbl->Release(pActionCollection);
    }
    if (pTaskDefinition) {
        pTaskDefinition->lpVtbl->Release(pTaskDefinition);
    }
    if (pTaskFolder) {
        pTaskFolder->lpVtbl->Release(pTaskFolder);
    }
    if (pTaskService) {
        pTaskService->lpVtbl->Release(pTaskService);
    }
    
    OLEAUT32$VariantClear(&Vhost);
    OLEAUT32$VariantClear(&VNull);
    OLE32$CoUninitialize();

	return actionResult;
}



int go(char *args, int len) {
	BOOL res = NULL;
	datap parser;
	
    WCHAR *taskName; 
	WCHAR *hostName  = L""; 
    WCHAR *programPath; 
    WCHAR *programArguments  = L""; 
	CHAR *triggerType; 
	WCHAR *startTime; 
    WCHAR *expireTime = L""; 
	int daysInterval = 0; 
	WCHAR *delay = L"";
	WCHAR *userID  = L""; 
	WCHAR *repeatTask = L"";
	WCHAR* choice;
	
	
	BeaconDataParse(&parser, args, len);
	choice = BeaconDataExtract(&parser, NULL);
	taskName = BeaconDataExtract(&parser, NULL);
	//hostName = BeaconDataExtract(&parser, NULL);
	programPath = BeaconDataExtract(&parser, NULL);
	//programArguments = BeaconDataExtract(&parser, NULL);
	triggerType = BeaconDataExtract(&parser, NULL);
	
	if (MSVCRT$strcmp(triggerType, "daily") == 0) {
		BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Task is during creation ...");
		startTime = BeaconDataExtract(&parser, NULL);
		expireTime = BeaconDataExtract(&parser, NULL);
		daysInterval = BeaconDataInt(&parser);
		delay = BeaconDataExtract(&parser, NULL);
		res = CreateScheduledTask(triggerType, taskName, hostName, programPath, programArguments, startTime, expireTime, daysInterval, delay, userID, repeatTask);
		BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Task is created :D");
		BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Thank you for you patience :D");
	} else {
		BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: %s", triggerType);
		BeaconPrintf(CALLBACK_ERROR, "csb BOF: Specified triggerType is not supported.\n");
	}

	return 0;
}