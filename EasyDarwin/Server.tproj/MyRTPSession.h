#pragma once
#include <vector>
#include <string>
#include <boost/utility/string_view.hpp>
#include "QTSS.h"
#include "RTPOverbufferWindow.h"

class MyRTSPRequest;
class MyRTPStream;
class MyRTPSession {
	std::vector<MyRTPStream*>       fStreamBuffer;
	RTPOverbufferWindow fOverbufferWindow;
	std::string         fRTSPSessionID;
	friend class MyRTPStream;
public:
	MyRTPSession() = default;
	~MyRTPSession() = default;
	//Once the session is bound, a module can add streams to it.
	//It must pass in a trackID that uniquely identifies this stream.
	//This call can only be made during an RTSP Setup request, and the
	//RTSPRequestInterface must be provided.
	//You may also opt to attach a codec name and type to this stream.
	QTSS_Error  AddStream(MyRTSPRequest* request, MyRTPStream** outStream,
		QTSS_AddStreamFlags inFlags);
	boost::string_view GetSessionID() const { return fRTSPSessionID; }
};