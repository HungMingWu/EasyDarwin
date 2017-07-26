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
	 File:       GenerateXMLPrefs.h

	 Contains:   Routine that updates a QTSS 1.x 2.x PrefsSource to the new XMLPrefsSource.
 */

#include "GenerateXMLPrefs.h"
#include "QTSSDataConverter.h"
#include "QTSS.h"

struct PrefConversionInfo
{
	char*               fPrefName;
	char*               fModuleName;
	QTSS_AttrDataType   fPrefType;
};

static const PrefConversionInfo kPrefs[] =
{
	{ "rtsp_timeout",							nullptr,	qtssAttrDataTypeUInt32 },
	{ "rtsp_session_timeout",					nullptr,	qtssAttrDataTypeUInt32 },
	{ "rtp_session_timeout",					nullptr,	qtssAttrDataTypeUInt32 },
	{ "maximum_connections",					nullptr,	qtssAttrDataTypeint32_t },
	{ "maximum_bandwidth",						nullptr,	qtssAttrDataTypeint32_t },
	{ "movie_folder",						nullptr,	qtssAttrDataTypeCharArray },
	{ "bind_ip_addr",							nullptr,	qtssAttrDataTypeCharArray },
	{ "break_on_assert",						nullptr,	qtssAttrDataTypeBool16 },
	{ "auto_restart",							nullptr,	qtssAttrDataTypeBool16 },
	{ "total_bytes_update",						nullptr,	qtssAttrDataTypeUInt32 },
	{ "average_bandwidth_update",				nullptr,	qtssAttrDataTypeUInt32 },
	{ "safe_play_duration",                     nullptr,   qtssAttrDataTypeUInt32 },
	{ "module_folder",                          nullptr,   qtssAttrDataTypeCharArray },
	{ "error_logfile_name",                     nullptr,   qtssAttrDataTypeCharArray },
	{ "error_logfile_dir",                      nullptr,   qtssAttrDataTypeCharArray },
	{ "error_logfile_interval",                 nullptr,   qtssAttrDataTypeUInt32 },
	{ "error_logfile_size",                     nullptr,   qtssAttrDataTypeUInt32 },
	{ "error_logfile_verbosity",                nullptr,   qtssAttrDataTypeUInt32 },
	{ "screen_logging",                         nullptr,   qtssAttrDataTypeBool16 },
	{ "error_logging",                          nullptr,   qtssAttrDataTypeBool16 },
	{ "drop_all_video_delay",                   nullptr,   qtssAttrDataTypeint32_t },
	{ "start_thinning_delay",                   nullptr,   qtssAttrDataTypeint32_t },
	{ "large_window_size",                      nullptr,   qtssAttrDataTypeint32_t },
	{ "window_size_threshold",                  nullptr,   qtssAttrDataTypeint32_t },
	{ "min_tcp_buffer_size",                    nullptr,   qtssAttrDataTypeUInt32 },
	{ "max_tcp_buffer_size",                    nullptr,   qtssAttrDataTypeUInt32 },
	{ "tcp_seconds_to_buffer",                  nullptr,   qtssAttrDataTypeFloat32 },
	{ "do_report_http_connection_ip_address",   nullptr,   qtssAttrDataTypeBool16 },
	{ "default_authorization_realm",            nullptr,   qtssAttrDataTypeCharArray },
	{ "run_user_name",                          nullptr,   qtssAttrDataTypeCharArray },
	{ "run_group_name",                         nullptr,   qtssAttrDataTypeCharArray },
	{ "append_source_addr_in_transport",        nullptr,   qtssAttrDataTypeBool16 },
	{ "rtsp_port",                              nullptr,   qtssAttrDataTypeUInt16 },

	// This element will be used if the pref is something we don't know about.
	// Just have unknown prefs default to be server prefs with a type of char
	{ nullptr,                                     nullptr,	qtssAttrDataTypeCharArray }
};

int GenerateAllXMLPrefs(FilePrefsSource* inPrefsSource, XMLPrefsParser* inXMLPrefs)
{
	for (uint32_t x = 0; x < inPrefsSource->GetNumKeys(); x++)
	{
		//
		// Get the name of this pref
		char* thePrefName = inPrefsSource->GetKeyAtIndex(x);

		//
		// Find the information corresponding to this pref in the above array
		uint32_t y = 0;
		for (; kPrefs[y].fPrefName != nullptr; y++)
			if (::strcmp(thePrefName, kPrefs[y].fPrefName) == 0)
				break;

		char* theTypeString = (char*)QTSSDataConverter::TypeToTypeString(kPrefs[y].fPrefType);
		ContainerRef module = inXMLPrefs->GetRefForModule(kPrefs[y].fModuleName);
		ContainerRef pref = inXMLPrefs->AddPref(module, thePrefName, theTypeString);

		char* theValue = inPrefsSource->GetValueAtIndex(x);

		static char* kTrue = "true";
		static char* kFalse = "false";

		//
		// If the pref is a bool, the new pref format uses "true" & "false",
		// the old one uses "enabled" and "disabled", so we have to explicitly convert.
		if (kPrefs[y].fPrefType == qtssAttrDataTypeBool16)
		{
			if (::strcmp(theValue, "enabled") == 0)
				theValue = kTrue;
			else
				theValue = kFalse;
		}
		inXMLPrefs->AddPrefValue(pref, theValue);
	}

	return inXMLPrefs->WritePrefsFile();
}

int GenerateStandardXMLPrefs(PrefsSource* inPrefsSource, XMLPrefsParser* inXMLPrefs)
{
	char thePrefBuf[1024];

	for (uint32_t x = 0; kPrefs[x].fPrefName != nullptr; x++)
	{
		char* theTypeString = (char*)QTSSDataConverter::TypeToTypeString(kPrefs[x].fPrefType);
		ContainerRef module = inXMLPrefs->GetRefForModule(kPrefs[x].fModuleName);
		ContainerRef pref = inXMLPrefs->AddPref(module, kPrefs[x].fPrefName, theTypeString);

		for (uint32_t y = 0; true; y++)
		{
			if (inPrefsSource->GetValueByIndex(kPrefs[x].fPrefName, y, thePrefBuf) == 0)
				break;

			//
			// If the pref is a bool, the new pref format uses "true" & "false",
			// the old one uses "enabled" and "disabled", so we have to explicitly convert.
			if (kPrefs[x].fPrefType == qtssAttrDataTypeBool16)
			{
				if (::strcmp(thePrefBuf, "enabled") == 0)
					::strcpy(thePrefBuf, "true");
				else
					::strcpy(thePrefBuf, "false");
			}
			inXMLPrefs->AddPrefValue(pref, thePrefBuf);
		}
	}

	return inXMLPrefs->WritePrefsFile();
}
