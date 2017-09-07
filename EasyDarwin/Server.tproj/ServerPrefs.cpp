#include <boost/algorithm/string/predicate.hpp>
#include "ServerPrefs.h"
namespace ServerPrefs {
	uint32_t GetSafePlayDurationInSecs() {
		constexpr uint32_t  fSafePlayDurationInSecs = 600;
		return fSafePlayDurationInSecs;
	}
	uint32_t GetTotalBytesUpdateTimeInSecs() {
		constexpr uint32_t  fTBUpdateTimeInSecs = 1;
		return fTBUpdateTimeInSecs;
	}
	uint32_t  GetRTCPSocketRcvBufSizeinK() {
		constexpr uint32_t  fRTCPSocketRcvBufSizeInK = 768;
		return fRTCPSocketRcvBufSizeInK;
	}
	uint32_t GetAvgBandwidthUpdateTimeInSecs() {
		constexpr uint32_t  fABUpdateTimeInSecs = 60;
		return fABUpdateTimeInSecs;
	}
	bool GetDisableThinning() {
		constexpr bool fDisableThinning = false;
		return fDisableThinning;
	}
	uint16_t GetDefaultStreamQuality() {
		constexpr uint16_t fDefaultStreamQuality = 0;
		return fDefaultStreamQuality;
	}
	bool GetAllowGuestDefault() {
		constexpr bool fAllowGuestAuthorizeDefault = true;
		return fAllowGuestAuthorizeDefault;
	}
	boost::string_view GetAuthorizationRealm()
	{
		boost::string_view realm("EasyDarwin");
		return realm;
	}
	bool IsSlowStartEnabled() {
		constexpr bool  fIsSlowStartEnabled = true;
		return fIsSlowStartEnabled;
	}
	uint32_t GetRTPSessionTimeoutInSecs() {
		constexpr uint32_t fRTPSessionTimeoutInSecs = 120;
		return fRTPSessionTimeoutInSecs;
	}
	uint32_t GetSendIntervalInMsec() {
		constexpr uint32_t fSendIntervalInMsec = 50;
		return fSendIntervalInMsec;
	}
	float GetOverbufferRate() {
		constexpr float	fOverbufferRate = 2.0;
		return fOverbufferRate;
	}
	uint32_t GetWindowSizeMaxThreshold() {
		constexpr uint32_t fWindowSizeMaxThreshold = 200;
		return fWindowSizeMaxThreshold;
	}
	uint32_t GetLargeWindowSizeInK() {
		constexpr uint32_t  fLargeWindowSizeInK = 64;
		return fLargeWindowSizeInK;
	}
	uint32_t GetWindowSizeThreshold() {
		constexpr uint32_t fWindowSizeThreshold = 200;
		return fWindowSizeThreshold;
	}
	uint32_t GetMediumWindowSizeInK() {
		constexpr uint32_t fMediumWindowSizeInK = 48;
		return fMediumWindowSizeInK;
	}
	uint32_t GetSmallWindowSizeInK() {
		constexpr uint32_t  fSmallWindowSizeInK = 24;
		return fSmallWindowSizeInK;
	}
	int32_t  GetMaxKBitsBandwidth() {
		constexpr int32_t fMaxBandwidthInKBits = 102400;
		return fMaxBandwidthInKBits;
	}
	uint32_t GetMaxRetransmitDelayInMsec() {
		constexpr uint32_t fMaxRetransDelayInMsec = 500;
		return fMaxRetransDelayInMsec;
	}
	uint32_t GetMaxSendAheadTimeInSecs() {
		constexpr uint32_t fMaxSendAheadTimeInSecs = 25;
		return fMaxSendAheadTimeInSecs;
	}
	bool GetUDPMonitorEnabled() {
		constexpr bool fUDPMonitorEnabled = false;
		return fUDPMonitorEnabled;
	}
	uint16_t GetUDPMonitorVideoPort() {
		constexpr uint16_t fUDPMonitorVideoPort = 5002;
		return fUDPMonitorVideoPort;
	}
	uint16_t GetUDPMonitorAudioPort() {
		constexpr uint16_t fUDPMonitorAudioPort = 5004;
		return fUDPMonitorAudioPort;
	}
	boost::string_view GetMonitorDestIP() {
		boost::string_view dstIP("127.0.0.1");
		return dstIP;
	}
	boost::string_view GetMonitorSrcIP() {
		boost::string_view srcIP("0.0.0.0");
		return srcIP;
	}
	uint32_t GetMaxTCPBufferSizeInBytes() {
		constexpr uint32_t fMaxTCPBufferSizeInBytes = 200000;
		return fMaxTCPBufferSizeInBytes;
	}
	uint32_t GetMinTCPBufferSizeInBytes() {
		constexpr uint32_t  fMinTCPBufferSizeInBytes = 8192;
		return fMinTCPBufferSizeInBytes;
	}
	uint32_t GetRTSPTimeoutInSecs() {
		constexpr uint32_t fRTSPTimeoutInSecs = 0;
		return fRTSPTimeoutInSecs;
	}

	// Thinning algorithm parameters
	int32_t GetDropAllVideoPacketsTimeInMsec() {
		constexpr int32_t fDropAllVideoPacketsTimeInMsec = 1750;
		return fDropAllVideoPacketsTimeInMsec;
	}
	int32_t GetDropAllPacketsTimeInMsec() {
		constexpr int32_t fDropAllPacketsTimeInMsec = 2500;
		return fDropAllPacketsTimeInMsec;
	}
	int32_t GetThinAllTheWayTimeInMsec() {
		constexpr int32_t fThinAllTheWayTimeInMsec = 1500;
		return fThinAllTheWayTimeInMsec;
	}
	int32_t GetAlwaysThinTimeInMsec() {
		constexpr int32_t fAlwaysThinTimeInMsec = 750;
		return fAlwaysThinTimeInMsec;
	}
	int32_t GetStartThinningTimeInMsec() {
		constexpr int32_t fStartThinningTimeInMsec = 0;
		return fStartThinningTimeInMsec;
	}
	int32_t GetStartThickingTimeInMsec() {
		constexpr int32_t fStartThickingTimeInMsec = 250;
		return fStartThickingTimeInMsec;
	}
	int32_t GetThickAllTheWayTimeInMsec() {
		constexpr int32_t fThickAllTheWayTimeInMsec = -2000;
		return fThickAllTheWayTimeInMsec;
	}
	uint32_t GetQualityCheckIntervalInMsec() {
		constexpr uint32_t fQualityCheckIntervalInMsec = 1000;
		return fQualityCheckIntervalInMsec;
	}
	//This is the real timeout
	uint32_t GetRTSPSessionTimeoutInSecs() {
		constexpr uint32_t fRTSPSessionTimeoutInSecs = 180;
		return fRTSPSessionTimeoutInSecs;
	}
	//
	// Transport addr pref. Caller must provide a buffer big enough for an IP addr
	boost::string_view GetTransportSrcAddr()
	{
		return {};
	}
	bool IsReliableUDPEnabled() {
		constexpr bool fReliableUDP = true;
		return fReliableUDP;
	}
	// for tcp buffer size scaling
	float GetTCPSecondsToBuffer() {
		constexpr float fTCPSecondsToBuffer = 0.5;
		return fTCPSecondsToBuffer;
	}
	bool IsPathInsideReliableUDPDir(boost::string_view inPath)
	{
		std::vector<std::string> dirs;
		for (const auto &dir : dirs)
			if (boost::iequals(dir, inPath))
				return true;
		return false;
	}
	boost::string_view GetMovieFolder()
	{
		return boost::string_view("./");
	}
	std::vector<std::string> GetReqRTPStartTimeAdjust()
	{
		return { "Real" };
	}
}