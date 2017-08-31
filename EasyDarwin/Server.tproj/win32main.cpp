/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
 /*
	 File:       win32main.cpp
	 Contains:   main function to drive streaming server on win32.
 */

#include <thread>
#include <boost/asio/io_service.hpp>
#include "getopt.h"
#include "RunServer.h"
#include "QTSServer.h"

boost::asio::io_service io_service;

 // Data
static uint16_t sPort = 0; //port can be set on the command line
static SERVICE_STATUS_HANDLE sServiceStatusHandle = 0;
static QTSS_ServerState sInitialState = qtssRunningState;

// Functions
static void ReportStatus(DWORD inCurrentState, DWORD inExitCode);
static void InstallService(char* inServiceName);
static void RemoveService(char *inServiceName);
static void RunAsService(char* inServiceName);
void WINAPI ServiceControl(DWORD);
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);

int main(int argc, char * argv[])
{
	std::thread t([&] {
		boost::asio::io_service::work work(io_service);
		io_service.run();
	});
	t.detach();
	extern char* optarg;
	char sAbsolutePath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, sAbsolutePath);
	//First thing to do is to read command-line arguments.
	int ch;

	char* theConfigFilePath = "./easydarwin.cfg";
	char* theXMLFilePath = "./easydarwin.xml";
	bool notAService = false;
	bool theXMLPrefsExist = true;
	bool dontFork = false;

#if _DEBUG
	char* compileType = "Compile_Flags/_DEBUG; ";
#else
	char* compileType = "Compile_Flags/_RELEASE;";
#endif

	printf("%s/%s ( Build/%s; Platform/%s; %s%s) Built on: %s\n",
		QTSServerInterface::GetServerName().data(),
		QTSServerInterface::GetServerVersion().data(),
		QTSServerInterface::GetServerBuild().data(),
		QTSServerInterface::GetServerPlatform().Ptr,
		compileType,
		QTSServerInterface::GetServerComment().data(),
		QTSServerInterface::GetServerBuildDate().data());


	while ((ch = getopt(argc, argv, "vdxp:o:c:irsS:I")) != EOF) // opt: means requires option
	{
		switch (ch)
		{
		case 'v':

			printf("%s/%s ( Build/%s; Platform/%s; %s%s) Built on: %s\n", 
				QTSServerInterface::GetServerName().data(),
				QTSServerInterface::GetServerVersion().data(),
				QTSServerInterface::GetServerBuild().data(),
				QTSServerInterface::GetServerPlatform().Ptr,
				compileType,
				QTSServerInterface::GetServerComment().data(),
				QTSServerInterface::GetServerBuildDate().data());

			printf("usage: %s [ -d | -p port | -v | -c /myconfigpath.xml | -o /myconfigpath.conf | -x | -S numseconds | -I | -h ]\n", QTSServerInterface::GetServerName().data());
			printf("-d: Don't run as a Win32 Service\n");
			printf("-p XXX: Specify the default RTSP listening port of the server\n");
			printf("-c c:\\myconfigpath.xml: Specify a config file path\n");
			printf("-o c:\\myconfigpath.conf: Specify a DSS 1.x / 2.x config file path\n");
			printf("-x: Force create new .xml config file from 1.x / 2.x config\n");
			printf("-i: Install the EasyDarwin service\n");
			printf("-r: Remove the EasyDarwin service\n");
			printf("-s: Start the EasyDarwin service\n");
			printf("-S n: Display server stats in the console every \"n\" seconds\n");
			printf("-I: Start the server in the idle state\n");
			::exit(0);
		case 'd':
			notAService = true;
			break;
		case 'p':
			Assert(optarg != NULL);// this means we didn't declare getopt options correctly or there is a bug in getopt.
			sPort = ::atoi(optarg);
			break;
		case 'c':
			Assert(optarg != NULL);// this means we didn't declare getopt options correctly or there is a bug in getopt.
			theXMLFilePath = optarg;
			break;
		case 'o':
			Assert(optarg != NULL);// this means we didn't declare getopt options correctly or there is a bug in getopt.
			theConfigFilePath = optarg;
			break;
		case 'x':
			theXMLPrefsExist = false; // Force us to generate a new XML prefs file
			break;
		case 'i':
			printf("Installing the EasyDarwin service...\n");
			::InstallService("EasyDarwin");
			printf("Starting the EasyDarwin service...\n");
			::RunAsService("EasyDarwin");
			::exit(0);
			break;
		case 'r':
			printf("Removing the EasyDarwin service...\n");
			::RemoveService("EasyDarwin");
			::exit(0);
		case 's':
			printf("Starting the EasyDarwin service...\n");
			::RunAsService("EasyDarwin");
			::exit(0);
		case 'I':
			sInitialState = qtssIdleState;
			break;
		default:
			break;
		}
	}

	//
	// Start Win32 DLLs
	WORD wsVersion = MAKEWORD(1, 1);
	WSADATA wsData;
	(void)::WSAStartup(wsVersion, &wsData);

	if (notAService)
	{
		// If we're running off the command-line, don't do the service initiation crap.
		::StartServer(sPort, sInitialState, false, sAbsolutePath); // No stats update interval for now
		::RunServer();
		::exit(0);
	}

	SERVICE_TABLE_ENTRY dispatchTable[] =
	{
		{ "", ServiceMain },
		{ NULL, NULL }
	};

	//
	// In case someone runs the server improperly, print out a friendly message.
	printf("EasyDarwin must either be started from the DOS Console\n");
	printf("using the -d command-line option, or using the Service Control Manager\n\n");
	printf("Waiting for the Service Control Manager to start EasyDarwin...\n");
	BOOL theErr = ::StartServiceCtrlDispatcher(dispatchTable);
	if (!theErr)
	{
		printf("Fatal Error: Couldn't start Service\n");
		::exit(-1);
	}

	return (0);
}
#if 0
bool GetWindowsServiceInstallPath(char* inPath)
{
	HKEY hKEY;
	if (RegOpenKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EasyDarwin", &hKEY) == ERROR_SUCCESS)
	{
		DWORD dwType;
		DWORD cchData;
		dwType = REG_EXPAND_SZ;
		char szName[MAX_PATH];
		cchData = sizeof(szName);
		if (RegQueryValueEx(hKEY, "ImagePath", NULL, &dwType, (LPBYTE)szName, &cchData) == ERROR_SUCCESS)
		{
			char* p = strstr(szName, "EasyDarwin.exe");
			if (p != NULL)
			{
				int len = p - szName;
				memcpy(inPath, szName, p - szName);
				inPath[len] = '\0';
				return 1;
			}
		}
		RegCloseKey(hKEY);
	}
	return 0;
}
#endif
void __stdcall ServiceMain(DWORD /*argc*/, LPTSTR *argv)
{
	char* theServerName = argv[0];
	char sAbsolutePath[MAX_PATH];
	::GetModuleFileName(NULL, sAbsolutePath, MAX_PATH);
	int cnt = strlen(sAbsolutePath);
	for (int i = cnt; i >= 0; --i)
	{
		if (sAbsolutePath[i] == '\\')
		{
			sAbsolutePath[i + 1] = '\0';
			break;
		}
	}
	sServiceStatusHandle = ::RegisterServiceCtrlHandler(theServerName, &ServiceControl);
	if (sServiceStatusHandle == 0)
	{
		printf("Failure registering service handler");
		return;
	}

	//
	// Report our status
	::ReportStatus(SERVICE_START_PENDING, NO_ERROR);


	//
	// Start & Run the server - no stats update interval for now
	if (::StartServer(sPort, sInitialState, false, sAbsolutePath) != qtssFatalErrorState)
	{
		::ReportStatus(SERVICE_RUNNING, NO_ERROR);
		::RunServer(); // This function won't return until the server has died

		//
		// Ok, server is done...
		::ReportStatus(SERVICE_STOPPED, NO_ERROR);
	}
	else
		::ReportStatus(SERVICE_STOPPED, ERROR_BAD_COMMAND); // I dunno... report some error

#ifdef WIN32
	::ExitProcess(0);
#else
	::exit(0);
#endif //WIN32
}

void WINAPI ServiceControl(DWORD inControlCode)
{
	QTSS_ServerState theState;
	QTSServerInterface* theServer = QTSServerInterface::GetServer();
	DWORD theStatusReport = SERVICE_START_PENDING;

	if (theServer != NULL)
		theState = theServer->GetServerState();
	else
		theState = qtssStartingUpState;

	switch (inControlCode)
	{
		// Stop the service.
		//
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		{
			if (theState == qtssStartingUpState)
				break;

			//
			// Signal the server to shut down.
			if (theServer != NULL)
				theServer->SetServerState(qtssShuttingDownState);
			break;
		}
	case SERVICE_CONTROL_PAUSE:
		{
			if (theState != qtssRunningState)
				break;

			//
			// Signal the server to refuse new connections.
			if (theServer != NULL)
				theServer->SetServerState(qtssRefusingConnectionsState);
		}
	case SERVICE_CONTROL_CONTINUE:
		{
			if (theState != qtssRefusingConnectionsState)
				break;

			//
			// Signal the server to refuse new connections.
			if (theServer != NULL)
				theServer->SetServerState(qtssRefusingConnectionsState);
			break;
		}
	case SERVICE_CONTROL_INTERROGATE:
		break; // Just update our status

	default:
		break;
	}

	if (theServer != NULL)
	{
		theState = theServer->GetServerState();

		//
		// Convert a QTSS state to a Win32 Service state
		switch (theState)
		{
		case qtssStartingUpState:           theStatusReport = SERVICE_START_PENDING;    break;
		case qtssRunningState:              theStatusReport = SERVICE_RUNNING;          break;
		case qtssRefusingConnectionsState:  theStatusReport = SERVICE_PAUSED;           break;
		case qtssFatalErrorState:           theStatusReport = SERVICE_STOP_PENDING;     break;
		case qtssShuttingDownState:         theStatusReport = SERVICE_STOP_PENDING;     break;
		default:                            theStatusReport = SERVICE_RUNNING;          break;
		}
	}
	else
		theStatusReport = SERVICE_START_PENDING;

	printf("Reporting status from ServiceControl function\n");
	::ReportStatus(theStatusReport, NO_ERROR);

}

void ReportStatus(DWORD inCurrentState, DWORD inExitCode)
{
	static bool sFirstTime = 1;
	static uint32_t sCheckpoint = 0;
	static SERVICE_STATUS sStatus;

	if (sFirstTime)
	{
		sFirstTime = false;

		//
		// Setup the status structure
		sStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		sStatus.dwCurrentState = SERVICE_START_PENDING;
		//sStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN;
		sStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		sStatus.dwWin32ExitCode = 0;
		sStatus.dwServiceSpecificExitCode = 0;
		sStatus.dwCheckPoint = 0;
		sStatus.dwWaitHint = 0;
	}

	if (sStatus.dwCurrentState == SERVICE_START_PENDING)
		sStatus.dwCheckPoint = ++sCheckpoint;
	else
		sStatus.dwCheckPoint = 0;

	sStatus.dwCurrentState = inCurrentState;
	sStatus.dwServiceSpecificExitCode = inExitCode;
	BOOL theErr = SetServiceStatus(sServiceStatusHandle, &sStatus);
	if (theErr == 0)
	{
		DWORD theerrvalue = ::GetLastError();
	}
}

void RunAsService(char* inServiceName)
{
	SC_HANDLE   theService;
	SC_HANDLE   theSCManager;

	theSCManager = ::OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
	);
	if (!theSCManager)
		return;

	theService = ::OpenService(
		theSCManager,               // SCManager database
		inServiceName,               // name of service
		SERVICE_ALL_ACCESS);

	SERVICE_STATUS lpServiceStatus;

	if (theService)
	{
		const int32_t kNotRunning = 1062;
		bool stopped = ::ControlService(theService, SERVICE_CONTROL_STOP, &lpServiceStatus);
		if (!stopped && ((int32_t) ::GetLastError() != kNotRunning))
			printf("Stopping Service Error: %d\n", ::GetLastError());

		bool started = ::StartService(theService, 0, NULL);
		if (!started)
			printf("Starting Service Error: %d\n", ::GetLastError());

		::CloseServiceHandle(theService);
	}

	::CloseServiceHandle(theSCManager);
}


void InstallService(char* inServiceName)
{
	SC_HANDLE   theService;
	SC_HANDLE   theSCManager;

	TCHAR thePath[512];
	TCHAR theQuotedPath[522];

	BOOL theErr = ::GetModuleFileName(NULL, thePath, 512);
	if (!theErr)
		return;

	sprintf(theQuotedPath, "\"%s\"", thePath);

	theSCManager = ::OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
	);
	if (!theSCManager)
	{
		printf("Failed to install EasyDarwin Service\n");
		return;
	}

	theService = CreateService(
		theSCManager,               // SCManager database
		inServiceName,               // name of service
		inServiceName,               // name to display
		SERVICE_ALL_ACCESS,         // desired access
		SERVICE_WIN32_OWN_PROCESS,  // service type
		SERVICE_AUTO_START,       // start type
		SERVICE_ERROR_NORMAL,       // error control type
		theQuotedPath,               // service's binary
		NULL,                       // no load ordering group
		NULL,                       // no tag identifier
		NULL,       // dependencies
		NULL,                       // LocalSystem account
		NULL);                      // no password

	if (theService)
	{
		::CloseServiceHandle(theService);
		printf("Installed EasyDarwin Service\n");
	}
	else
		printf("Failed to install EasyDarwin Service\n");

	::CloseServiceHandle(theSCManager);
}

void RemoveService(char *inServiceName)
{
	SC_HANDLE   theSCManager;
	SC_HANDLE   theService;

	theSCManager = ::OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
	);
	if (!theSCManager)
	{
		printf("Failed to remove EasyDarwin Service\n");
		return;
	}

	theService = ::OpenService(theSCManager, inServiceName, SERVICE_ALL_ACCESS);
	if (theService != NULL)
	{
		bool stopped = ::ControlService(theService, SERVICE_CONTROL_STOP, NULL);
		if (!stopped)
			printf("Stopping Service Error: %d\n", ::GetLastError());

		(void)::DeleteService(theService);
		::CloseServiceHandle(theService);
		printf("Removed EasyDarwin Service\n");
	}
	else
		printf("Failed to remove EasyDarwin Service\n");

	::CloseServiceHandle(theSCManager);
}
