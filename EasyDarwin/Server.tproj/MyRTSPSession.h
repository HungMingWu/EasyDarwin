#pragma once
#include <memory>
#include <string>
#include <system_error>
#include <boost/utility/string_view.hpp>
class RTSPServer;
class MyReflectorSession;
class RTPSession;
class MyRTPSession;
class RTPSessionOutput1;
class Connection;
class MyRTSPRequest;
class MyRTSPSession {
	RTSPServer& mServer;
	std::shared_ptr<MyReflectorSession> CreateSession(MyRTSPRequest &request, boost::string_view sessionName);
public:
	std::shared_ptr<MyRTPSession>     fRTPSession;
	std::shared_ptr<MyReflectorSession> broadcastSession;
	std::shared_ptr<RTPSessionOutput1> rtp_OutputSession;
	std::string fSessionID;
	std::vector<std::string> fChNumToSessIDMap;
	friend class Response;
	friend class RTSPServer;
public:
	MyRTSPSession(RTSPServer&) noexcept;
	~MyRTSPSession()
	{
		int a = 1;
	}
	std::error_code do_setup(MyRTSPRequest &request);
	std::error_code do_play(MyReflectorSession *session);
	std::error_code process_rtppacket(const char *packetData, size_t length);
	void FindOrCreateRTPSession(MyRTSPRequest &request);

	// If RTP data is interleaved into the RTSP connection, we need to associate
	// 2 unique channel numbers with each RTP stream, one for RTP and one for RTCP.
	// This function allocates 2 channel numbers, returns the lower one. The other one
	// is implicitly 1 greater.
	//
	// Pass in the RTSP Session ID of the Client session to which these channel numbers will
	// belong.
	uint8_t               GetTwoChannelNumbers(boost::string_view inRTSPSessionID);
};