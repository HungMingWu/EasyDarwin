#pragma once
#include <memory>
#include <string>
#include <boost/utility/string_view.hpp>
class RTSPServer;
class ReflectorSession;
class RTPSession;
class MyRTPSession;
class RTPSessionOutput1;
class Connection;
class MyRTSPRequest;
class MyRTSPSession {
	RTSPServer& mServer;
	std::shared_ptr<ReflectorSession> CreateSession(boost::string_view sessionName);
	std::shared_ptr<MyRTPSession>     fRTPSession;
	std::string fSessionID;
public:
	MyRTSPSession(RTSPServer&, std::shared_ptr<Connection> connection) noexcept;
	void do_setup();
	void FindOrCreateRTPSession();
	std::shared_ptr<ReflectorSession> broadcastSession;
	std::shared_ptr<RTPSessionOutput1> rtp_OutputSession;
	std::shared_ptr<Connection> connection;
	std::shared_ptr<MyRTSPRequest> request;
};