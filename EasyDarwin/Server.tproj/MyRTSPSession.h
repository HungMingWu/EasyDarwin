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
	std::shared_ptr<MyReflectorSession> CreateSession(boost::string_view sessionName);
	std::shared_ptr<MyRTPSession>     fRTPSession;
	std::shared_ptr<MyReflectorSession> broadcastSession;
	std::shared_ptr<RTPSessionOutput1> rtp_OutputSession;
	std::shared_ptr<Connection> connection;
	std::shared_ptr<MyRTSPRequest> request;
	std::string fSessionID;
	std::vector<std::string> fChNumToSessIDMap;
	friend class Response;
	friend class RTSPServer;
public:
	MyRTSPSession(RTSPServer&, std::shared_ptr<Connection> connection) noexcept;
	~MyRTSPSession()
	{
		int a = 1;
	}
	std::error_code do_setup();
	std::error_code do_play(MyReflectorSession *session);
	std::error_code process_rtppacket(const char *packetData, size_t length);
	void FindOrCreateRTPSession();

	// If RTP data is interleaved into the RTSP connection, we need to associate
	// 2 unique channel numbers with each RTP stream, one for RTP and one for RTCP.
	// This function allocates 2 channel numbers, returns the lower one. The other one
	// is implicitly 1 greater.
	//
	// Pass in the RTSP Session ID of the Client session to which these channel numbers will
	// belong.
	uint8_t               GetTwoChannelNumbers(boost::string_view inRTSPSessionID);
};