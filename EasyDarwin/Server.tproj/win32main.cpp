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

#include <mutex>
#include <thread>
#include <iostream>
#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include "RunServer.h"
#include "QTSServer.h"
#include "RTSPServer.h"

boost::asio::io_service io_service;

 // Data
static uint16_t sPort = 0; //port can be set on the command line
static QTSS_ServerState sInitialState = qtssRunningState;

int main(int argc, char * argv[])
{
	RTSPServer listener(io_service);
	std::thread t([&] {
		boost::asio::io_service::work work(io_service);
		io_service.run();
	});
	t.detach();

	char sAbsolutePath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, sAbsolutePath);
	//First thing to do is to read command-line arguments.

	//
	// Start Win32 DLLs
	WORD wsVersion = MAKEWORD(1, 1);
	WSADATA wsData;
	(void)::WSAStartup(wsVersion, &wsData);

	::StartServer(sPort, sInitialState, false, sAbsolutePath); // No stats update interval for now
	::RunServer();
	::exit(0);

	return (0);
}