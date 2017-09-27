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
	 File:       QTSServerInterface.cpp
	 Contains:   Implementation of object defined in QTSServerInterface.h
 */

 //INCLUDES:

#include <boost/asio/io_service.hpp>

#include "QTSServerInterface.h"

#include "RTPSessionInterface.h"
#include "OSRef.h"
#include "UDPSocketPool.h"
#include "RTSPProtocol.h"
#include "ServerPrefs.h"

// STATIC DATA

static QTSServerInterface* sServer = nullptr;

boost::string_view      QTSServerInterface::sServerBuildDateStr(__DATE__ ", " __TIME__);

std::string             QTSServerInterface::sPublicHeaderStr;

QTSServerInterface::QTSServerInterface()
{
	sServer = this;
}

QTSServerInterface* getSingleton()
{
	return sServer;
}

void QTSServerInterface::KillAllRTPSessions()
{
	OSMutexLocker locker(fRTPMap->GetMutex());
	for (OSRefHashTableIter theIter(fRTPMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
	{
		OSRef* theRef = theIter.GetCurrent();
		auto* theSession = (RTPSessionInterface*)theRef->GetObject();
		theSession->Signal(Task::kKillEvent);
	}
}