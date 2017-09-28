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
	 File:       RTPSessionInterface.h

	 Contains:   Implementation of object defined in .h
 */

#include <memory>
#include <random>
#include "RTPSessionInterface.h"
#include "QTSServerInterface.h"
#include "RTSPRequestInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "RTPStream.h"
#include "ServerPrefs.h"

RTPSessionInterface::RTPSessionInterface()
	: Task(),
	// assume true until proven false!
	fTimeoutTask(nullptr, ServerPrefs::GetRTPSessionTimeoutInSecs() * 1000)
{
	//don't actually setup the fTimeoutTask until the session has been bound!
	//(we don't want to get timeouts before the session gets bound)

	fTimeoutTask.SetTask(this);
}

void RTPSessionInterface::UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession)
{
	if (inNewRTSPSession != fRTSPSession)
	{
		// If there was an old session, let it know that we are done
		if (fRTSPSession != nullptr)
			fRTSPSession->DecrementObjectHolderCount();

		// Increment this count to prevent the RTSP session from being deleted
		fRTSPSession = inNewRTSPSession;
		fRTSPSession->IncrementObjectHolderCount();
	}
}