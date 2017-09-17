#include "MyReflectorSession.h"
#include "MyReflectorStream.h"
#include "MyReflectorSocket.h"

MyReflectorSession::MyReflectorSession(boost::string_view inSourceID, const SDPSourceInfo &inInfo)
	: fSessionName(inSourceID),
	fSourceInfo(inInfo)
{
}

void MyReflectorSession::AddBroadcasterClientSession(MyRTPSession* inClientSession)
{
	for (auto &stream : fStreamArray)
	{
		stream->GetSocketPair()->GetSocketA()->AddBroadcasterSession(inClientSession);
		stream->GetSocketPair()->GetSocketB()->AddBroadcasterSession(inClientSession);
	}
}

QTSS_Error MyReflectorSession::SetupReflectorSession(MyRTSPRequest &inRequest, MyRTPSession &inSession, uint32_t inFlags, bool filterState, uint32_t filterTimeout)
{
	// this must be set to the new SDP.
	fLocalSDP = fSourceInfo.GetLocalSDP();

	fStreamArray.resize(fSourceInfo.GetNumStreams());

	for (uint32_t x = 0; x < fSourceInfo.GetNumStreams(); x++)
	{
		fStreamArray[x] = std::make_unique<MyReflectorStream>(fSourceInfo.GetStreamInfo(x));
		// Obviously, we may encounter an error binding the reflector sockets.
		// If that happens, we'll just abort here, which will leave the ReflectorStream
		// array in an inconsistent state, so we need to make sure in our cleanup
		// code to check for NULL.
		QTSS_Error theError = fStreamArray[x]->BindSockets(inRequest, inSession, inFlags, filterState, filterTimeout);
		if (theError != QTSS_NoErr)
		{
			fStreamArray[x] = nullptr;
			return theError;
		}
		fStreamArray[x]->SetMyReflectorSession(this);

		fStreamArray[x]->SetEnableBuffer(this->fHasBufferedStreams);// buffering is done by the stream's sender

																	// If the port was 0, update it to reflect what the actual RTP port is.
		fSourceInfo.GetStreamInfo(x)->fPort = fStreamArray[x]->GetStreamInfo()->fPort;
		//printf("ReflectorSession::SetupReflectorSession fSourceInfo->GetStreamInfo(x)->fPort= %u\n",fSourceInfo->GetStreamInfo(x)->fPort);   
	}

	if (inFlags & kMarkSetup)
		fIsSetup = true;

	return QTSS_NoErr;
}