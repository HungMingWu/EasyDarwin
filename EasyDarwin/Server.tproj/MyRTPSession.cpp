#include "MyRTPSession.h"
#include "MyAssert.h"
#include "MyRTPStream.h"
#include "ServerPrefs.h"
MyRTPSession::MyRTPSession() :
	fOverbufferWindow(ServerPrefs::GetSendIntervalInMsec(), UINT32_MAX, ServerPrefs::GetMaxSendAheadTimeInSecs(),
		ServerPrefs::GetOverbufferRate())
{

}
void MyRTPSession::AddStream(MyRTSPRequest& request,
	QTSS_AddStreamFlags inFlags)
{
	// Create a new SSRC for this stream. This should just be a random number unique
	// to all the streams in the session
	uint32_t theSSRC = 0;
	while (theSSRC == 0)
	{
		theSSRC = (int32_t)::rand();

		for (const auto &theStream : fStreamBuffer)
		{
			if (theStream.GetSSRC() == theSSRC)
				theSSRC = 0;
		}
	}

	fStreamBuffer.emplace_back(request, theSSRC, *this, inFlags);
}