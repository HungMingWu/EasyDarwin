#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <boost/utility/string_view.hpp>
#include "QTSS.h"
namespace ServerPrefs {
	uint32_t GetSafePlayDurationInSecs();
	uint32_t GetTotalBytesUpdateTimeInSecs();
	uint32_t GetRTCPSocketRcvBufSizeinK();
	uint32_t GetAvgBandwidthUpdateTimeInSecs();
	bool GetAllowGuestDefault();
	boost::string_view GetAuthorizationRealm();
	uint32_t GetRTPSessionTimeoutInSecs();
	uint16_t GetDefaultStreamQuality();
	int32_t  GetMaxKBitsBandwidth();
	uint32_t GetMaxRetransmitDelayInMsec();
	int32_t GetDropAllPacketsTimeInMsec();
	uint32_t GetRTSPSessionTimeoutInSecs();
	boost::string_view GetTransportSrcAddr();
	float GetTCPSecondsToBuffer();
	boost::string_view GetMovieFolder();
	std::vector<std::string> GetReqRTPStartTimeAdjust();
}