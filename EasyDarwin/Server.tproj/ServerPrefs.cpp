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
	uint32_t GetMaxTCPBufferSizeInBytes() {
		constexpr uint32_t fMaxTCPBufferSizeInBytes = 200000;
		return fMaxTCPBufferSizeInBytes;
	}
	uint32_t GetMinTCPBufferSizeInBytes() {
		constexpr uint32_t  fMinTCPBufferSizeInBytes = 8192;
		return fMinTCPBufferSizeInBytes;
	}

	// Thinning algorithm parameters
	int32_t GetStartThinningTimeInMsec() {
		constexpr int32_t fStartThinningTimeInMsec = 0;
		return fStartThinningTimeInMsec;
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
	// for tcp buffer size scaling
	float GetTCPSecondsToBuffer() {
		constexpr float fTCPSecondsToBuffer = 0.5;
		return fTCPSecondsToBuffer;
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