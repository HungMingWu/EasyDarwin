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
	bool GetDisableThinning();
	bool GetAllowGuestDefault();
	boost::string_view GetAuthorizationRealm();
	bool IsSlowStartEnabled();
	uint32_t GetRTPSessionTimeoutInSecs();
	uint32_t GetSendIntervalInMsec();
	float GetOverbufferRate();
	uint16_t GetDefaultStreamQuality();
	uint32_t GetWindowSizeMaxThreshold();
	uint32_t GetLargeWindowSizeInK();
	uint32_t GetWindowSizeThreshold();
	uint32_t GetMediumWindowSizeInK();
	uint32_t GetSmallWindowSizeInK();
	int32_t  GetMaxKBitsBandwidth();
	uint32_t GetMaxRetransmitDelayInMsec();
	uint32_t GetMaxSendAheadTimeInSecs();
	bool GetUDPMonitorEnabled();
	uint16_t GetUDPMonitorVideoPort();
	uint16_t GetUDPMonitorAudioPort();
	boost::string_view GetMonitorDestIP();
	boost::string_view GetMonitorSrcIP();
	bool GetRTSPServerInfoEnabled();
	uint32_t GetMaxTCPBufferSizeInBytes();
	uint32_t GetMinTCPBufferSizeInBytes();
	uint32_t GetRTSPTimeoutInSecs();
	int32_t GetDropAllVideoPacketsTimeInMsec();
	int32_t GetDropAllPacketsTimeInMsec();
	int32_t GetThinAllTheWayTimeInMsec();
	int32_t GetAlwaysThinTimeInMsec();
	int32_t GetStartThinningTimeInMsec();
	int32_t GetStartThickingTimeInMsec();
	int32_t GetThickAllTheWayTimeInMsec();
	uint32_t GetQualityCheckIntervalInMsec();
	uint32_t GetRTSPSessionTimeoutInSecs();
	boost::string_view GetTransportSrcAddr();
	bool IsReliableUDPEnabled();
	float GetTCPSecondsToBuffer();
	bool IsPathInsideReliableUDPDir(boost::string_view inPath);
	boost::string_view GetMovieFolder();
	std::vector<std::string> GetReqRTPStartTimeAdjust();
}