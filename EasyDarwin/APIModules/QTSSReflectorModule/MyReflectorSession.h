#pragma once
#include <memory>
#include <string>
#include <vector>
#include "SDPSourceInfo.h"
#include "MyReflectorStream.h"

class MyRTPSession;
class MyRTSPRequest;
class MyReflectorSession {
	std::string	fSessionName;
	SDPSourceInfo fSourceInfo;
	std::string fLocalSDP;
	bool fHasBufferedStreams{ true };
	bool fIsSetup{ false };
	std::vector<std::unique_ptr<MyReflectorStream>>   fStreamArray;
public:
	enum
	{
		kMarkSetup = 1,     //After SetupReflectorSession is called, IsSetup returns true
		kDontMarkSetup = 2, //After SetupReflectorSession is called, IsSetup returns false
		kIsPushSession = 4  // When setting up streams handle port conflicts by allocating.
	};
	MyReflectorSession(boost::string_view inSourceID, const SDPSourceInfo &inInfo);
	QTSS_Error SetupReflectorSession(MyRTSPRequest &inRequest, MyRTPSession &inSession,
		uint32_t inFlags = kMarkSetup, bool filterState = true, uint32_t filterTimeout = 30);
	void AddBroadcasterClientSession(MyRTPSession* inClientSession);
	const SDPSourceInfo& GetSourceInfo() const { return fSourceInfo; }
	MyReflectorStream& GetStreamByIndex(uint32_t inIndex) { return *fStreamArray[inIndex]; }
};