#include "MyRTPSession.h"
#include "MyAssert.h"
#include "MyRTPStream.h"
#include "ServerPrefs.h"
MyRTPSession::MyRTPSession() :
	fOverbufferWindow(ServerPrefs::GetSendIntervalInMsec(), UINT32_MAX, ServerPrefs::GetMaxSendAheadTimeInSecs(),
		ServerPrefs::GetOverbufferRate())
{

}
QTSS_Error MyRTPSession::AddStream(MyRTSPRequest& request, MyRTPStream** outStream,
	QTSS_AddStreamFlags inFlags)
{
	Assert(outStream != nullptr);

	// Create a new SSRC for this stream. This should just be a random number unique
	// to all the streams in the session
	uint32_t theSSRC = 0;
	while (theSSRC == 0)
	{
		theSSRC = (int32_t)::rand();

		for (auto theStream : fStreamBuffer)
		{
			if (theStream->GetSSRC() == theSSRC)
				theSSRC = 0;
		}
	}

	*outStream = new MyRTPStream(theSSRC, *this);

	QTSS_Error theErr = (*outStream)->Setup(request, inFlags);
	if (theErr != QTSS_NoErr)
		// If we couldn't setup the stream, make sure not to leak memory!
		delete *outStream;
	else
	{
		// If the stream init succeeded, then put it into the array of setup streams
		fStreamBuffer.push_back(*outStream);
	}
	return theErr;
}
