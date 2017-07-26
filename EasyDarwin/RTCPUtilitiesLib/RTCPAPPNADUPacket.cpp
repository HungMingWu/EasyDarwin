/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
 /*
	 File:       RTCPAPPNADUPacket.cpp

	 Contains:   RTCPAPPNADUPacket de-packetizing classes


 */


#include "RTCPAPPNADUPacket.h"
#include "MyAssert.h"
#include "StrPtrLen.h"


 /* RTCPNaduPacket
 data: One or more of the following data format blocks may appear

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |V=2|P| subtype |   PT=APP=204  |             length            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           SSRC/CSRC                           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                          name (ASCII)                         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ <-------- data block
 |                            SSRC                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |      Playout Delay            |            NSN                |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  Reserved           |   NUN   |    Free Buffer Space (FBS)    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 */
char RTCPNaduPacket::sRTCPTestBuffer[];


RTCPNaduPacket::RTCPNaduPacket(bool debug) :
	RTCPAPPPacket(debug)
{
}

void RTCPNaduPacket::GetTestPacket(StrPtrLen* resultPtr)
{
	/*
	Compound test packet

	lengths are 32 bit words, include header, are minus 1

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|V=2|P|    RC   |   PT=RR=201   |             length            |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|V=2|P| subtype |   PT=SDES=202  |             length            |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|V=2|P| subtype |   PT=APP=204  |             length            |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           SSRC/CSRC                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                          name (ASCII)                         |  PSS0
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+----app specific data PSS0
	|                    SSRC                                       |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|      Playout Delay            |            NSN                |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Reserved           |   NUN   |    Free Buffer Space (FBS)    |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ ----app data may repeat



	// rtcp common header
		typedef struct {
		   unsigned int version:2;   // protocol version
		   unsigned int p:1;         // padding flag
		   unsigned int count:5;     // varies by packet type
		   unsigned int pt:8;        // RTCP packet type
		   u_int16 length;           // pkt len in words, w/o this word can be 0
	   } rtcp_common_t;

	 // rtcp compound packet starts with rtcp rr header
	 // rr data may be empty or not
	 // nadu app header follows rr header and data if any


	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/

#if 1 //full receiver report with SDES and Nadu
	auto  *theWriterStart = (uint32_t*)sRTCPTestBuffer;
	auto  *theWriter = (uint32_t*)sRTCPTestBuffer;

	*(theWriter++) = htonl(0x81c90007);     // 1 RR  packet header, full report
	*(theWriter++) = htonl(0x2935F2D6);     // 1 Sender SSRC = 691401430
	*(theWriter++) = htonl(0x6078CE22);     // 1 SSRC_1 = 1618529826
	*(theWriter++) = htonl(0x01000001);     // fraction lost | cumulative num packets lost 1% , 1 packet
	*(theWriter++) = htonl(0x0000361A);     // extended highest seq number received = 13850
	*(theWriter++) = htonl(0x00C7ED4D);     // interarrival jitter = 13102413
	*(theWriter++) = htonl(0x00000000);     // LSR last sender report = 0
	*(theWriter++) = htonl(0x04625238);     // Delay since last SR (DLSR) = 73552440 (garbage)

	*(theWriter++) = htonl(0x81ca0005);     // 1 SDES  packet header,
	*(theWriter++) = htonl(0x2935F2D6);     // 1 Sender SSRC = 691401430
	*(theWriter++) = htonl(0x010A5344);     // 1 CNAME = 01, len=10, "SD"
	*(theWriter++) = htonl(0x45532043);     // 1 CNAME = "ES C"
	*(theWriter++) = htonl(0x4e414d45);     // 1 CNAME = "NAME"
	*(theWriter++) = htonl(0x00000000);     // NULL item = end of list + 32bit padding



	*(theWriter++) = htonl(0x80CC0000);     // 1 APP packet header, needs len -> assigned beow

	uint32_t  *appPacketLenStart = theWriter;

	*(theWriter++) = htonl(FOUR_CHARS_TO_INT('S', 'S', 'R', 'C')); //nadu ssrc
	*(theWriter++) = htonl(FOUR_CHARS_TO_INT('P', 'S', 'S', '0')); //nadu app packet name

	// first (typically only) ssrc block
	*(theWriter++) = htonl(0x423A35C7); //ssrc = 1111111111
	*(theWriter++) = htonl(0x2B6756CE); //delay | nsn = 11111 | 22222
	*(theWriter++) = htonl(0xFFFFAD9C); //nun | fbs= 31 | 44444

	// optional 2nd or more ssrc blocks
	*(theWriter++) = htonl(0x84746B8E); //ssrc = 222222222
	*(theWriter++) = htonl(0x2B6756CE); //delay | nsn = 11111 | 22222
	*(theWriter++) = htonl(0xFFFFAD9C); //nun | fbs= 31 | 44444

	uint16_t *packetLenOffsetPtr = &((uint16_t*)theWriterStart)[29];
	uint16_t  packetLenInWords = htons(((uint32_t*)theWriter - (uint32_t*)appPacketLenStart));

	*packetLenOffsetPtr = packetLenInWords;
	printf("packetLenInWords =%lu\n", ntohs(packetLenInWords));
	uint32_t len = (char*)theWriter - (char*)theWriterStart;
	if (resultPtr)
		resultPtr->Set(sRTCPTestBuffer, len);

#endif

#if 0 //full receiver report with Nadu
	uint32_t  *theWriterStart = (uint32_t*)sRTCPTestBuffer;
	uint32_t  *theWriter = (uint32_t*)sRTCPTestBuffer;

	*(theWriter++) = htonl(0x80c90007);     // 1 RR  packet header, empty len is ok but could be a full report
	*(theWriter++) = htonl(0x2935F2D6);     // 1 SSRC = 691401430
	*(theWriter++) = htonl(0x6078CE22);     // 1 SSRC_1 = 1618529826
	*(theWriter++) = htonl(0x01000001);     // fraction lost | cumulative num packets lost 1% , 1 packet
	*(theWriter++) = htonl(0x0000361A);     // extended highest seq number received = 13850
	*(theWriter++) = htonl(0x00C7ED4D);     // interarrival jitter = 13102413
	*(theWriter++) = htonl(0x00000000);     // LSR last sender report = 0
	*(theWriter++) = htonl(0x04625238);     // Delay since last SR (DLSR) = 73552440 (garbage)



	*(theWriter++) = htonl(0x80CC0000);     // 1 APP packet header, needs len -> assigned beow

	uint32_t  *appPacketLenStart = theWriter;

	*(theWriter++) = htonl(FOUR_CHARS_TO_INT('S', 'S', 'R', 'C')); //nadu ssrc
	*(theWriter++) = htonl(FOUR_CHARS_TO_INT('P', 'S', 'S', '0')); //nadu app packet name

	// first (typically only) ssrc block
	*(theWriter++) = htonl(0x423A35C7); //ssrc = 1111111111
	*(theWriter++) = htonl(0x2B6756CE); //delay | nsn = 11111 | 22222
	*(theWriter++) = htonl(0xFFFFAD9C); //nun | fbs= 31 | 44444

	// optional 2nd or more ssrc blocks
	*(theWriter++) = htonl(0x84746B8E); //ssrc = 222222222
	*(theWriter++) = htonl(0x2B6756CE); //delay | nsn = 11111 | 22222
	*(theWriter++) = htonl(0xFFFFAD9C); //nun | fbs= 31 | 44444

	uint16_t *packetLenOffsetPtr = &((uint16_t*)theWriterStart)[17];
	uint16_t  packetLenInWords = htons((uint32_t*)theWriter - (uint32_t*)appPacketLenStart);

	*packetLenOffsetPtr = packetLenInWords;

	uint32_t len = (char*)theWriter - (char*)theWriterStart;
	if (resultPtr)
		resultPtr->Set(sRTCPTestBuffer, len);

#endif

#if 0 //empty receiver report with NADU
	uint32_t  *theWriterStart = (uint32_t*)sRTCPTestBuffer;
	uint32_t  *theWriter = (uint32_t*)sRTCPTestBuffer;

	*(theWriter++) = htonl(0x80c90000);     // 1 RR  packet header, empty len is ok but could be a full report

	*(theWriter++) = htonl(0x80CC0000);     // 1 APP packet header, needs len -> assigned beow

	uint32_t  *appPacketLenStart = theWriter;

	*(theWriter++) = htonl(FOUR_CHARS_TO_INT('S', 'S', 'R', 'C')); //nadu ssrc
	*(theWriter++) = htonl(FOUR_CHARS_TO_INT('P', 'S', 'S', '0')); //nadu app packet name

	// first (typically only) ssrc block
	*(theWriter++) = htonl(0x423A35C7); //ssrc = 1111111111
	*(theWriter++) = htonl(0x2B6756CE); //delay | nsn = 11111 | 22222
	*(theWriter++) = htonl(0xFFFFAD9C); //nun | fbs= 31 | 44444

	// optional 2nd or more ssrc blocks
	*(theWriter++) = htonl(0x84746B8E); //ssrc = 222222222
	*(theWriter++) = htonl(0x2B6756CE); //delay | nsn = 11111 | 22222
	*(theWriter++) = htonl(0xFFFFAD9C); //nun | fbs= 31 | 44444

	uint16_t *packetLenOffsetPtr = &((uint16_t*)theWriterStart)[3];
	uint16_t  packetLenInWords = htons((uint32_t*)theWriter - (uint32_t*)appPacketLenStart);

	*packetLenOffsetPtr = packetLenInWords;

	uint32_t len = (char*)theWriter - (char*)theWriterStart;
	if (resultPtr)
		resultPtr->Set(sRTCPTestBuffer, len);
#endif

	/*

	sample run of the test packet below:
	----------------------------------------
	RTPStream::TestRTCPPackets received packet inPacketPtr.Ptr=0xf0080568 inPacketPtr.len =20
	testing RTCPNaduPacket using packet inPacketPtr.Ptr=0xe2c38 inPacketPtr.len =40
	>recv sess=1: RTCP RR recv_sec=6.812  type=video size=40 H_vers=2, H_pad=0, H_rprt_count=0, H_type=201, H_length=0, H_ssrc=-2134114296
	>recv sess=1: RTCP APP recv_sec=6.813  type=video size=36 H_vers=2, H_pad=0, H_rprt_count=0, H_type=204, H_length=8, H_ssrc=1397969475

	NADU Packet
		 Block Index = 0 (h_ssrc = 1111111111, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)
		 Block Index = 1 (h_ssrc = 2222222222, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)

	Dumping Nadu List (list size = 3  record count=48)
	-------------------------------------------------------------
	NADU Record: list_index = 2 list_recordID = 48
		 Block Index = 0 (h_ssrc = 1111111111, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)
		 Block Index = 1 (h_ssrc = 2222222222, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)
	NADU Record: list_index = 1 list_recordID = 47
		 Block Index = 0 (h_ssrc = 1111111111, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)
		 Block Index = 1 (h_ssrc = 2222222222, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)
	NADU Record: list_index = 0 list_recordID = 46
		 Block Index = 0 (h_ssrc = 1111111111, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)
		 Block Index = 1 (h_ssrc = 2222222222, h_playoutdelay = 11111, h_sequence_num = 22222, h_nun_unit_num = 31, h_fbs_free_buf = 44444)


	*/

}



// use if you don't know what kind of packet this is
bool RTCPNaduPacket::ParseNaduPacket(uint8_t* inPacketBuffer, uint32_t inPacketLength)
{

	if (!this->ParseAPPPacket(inPacketBuffer, inPacketLength))
		return false;

	if (this->GetAppPacketName() != RTCPNaduPacket::kNaduPacketName)
		return false;

	return true;
}


bool RTCPNaduPacket::ParseAPPData(uint8_t* inPacketBuffer, uint32_t inPacketLength)
{

	if (!this->ParseNaduPacket(inPacketBuffer, inPacketLength))
		return false;

	auto *naduDataBuffer = (uint32_t *)(this->GetPacketBuffer() + kNaduDataOffset);

	int wordsLen = this->GetPacketLength() - 2;
	if (wordsLen < 3) // min is 3
		return false;

	if (0 != (wordsLen % 3))// blocks are 3x32bits so there is a bad block somewhere.
		return false;

	fNumBlocks = wordsLen / 3;

	if (0 == fNumBlocks)
		return false;

	if (fNumBlocks > 100) // too many
		return false;

	fNaduDataBuffer = naduDataBuffer;

	if (0) //testing 
		this->DumpNaduPacket();

	return true;

}

void RTCPNaduPacket::DumpNaduPacket()
{
	char   printName[5];
	(void) this->GetAppPacketName(printName, sizeof(printName));
	printf(" H_app_packet_name = %s, ", printName);

	printf("\n");
	int32_t count = 0;
	for (; count < fNumBlocks; count++)
	{

		uint32_t ssrc = this->GetSSRC(count);
		uint32_t ssrcIndex = this->GetSSRCBlockIndex(ssrc);
		uint16_t playoutDelay = this->GetPlayOutDelay(count);
		uint16_t nsn = this->GetNSN(count);
		uint16_t nun = this->GetNUN(count);
		uint16_t fbs = this->GetFBS(count);
		printf("              ");
		printf("RTCP APP NADU Report[%"   _U32BITARG_   "] ", ssrcIndex);
		printf("h_ssrc = %"   _U32BITARG_, ssrc);
		printf(", h_playoutdelay = %u", playoutDelay);
		printf(", h_sequence_num = %u", nsn);
		printf(", h_nun_unit_num = %u", nun);
		printf(", h_fbs_free_buf = %u", fbs);

		printf("\n");
	}
}




int32_t RTCPNaduPacket::GetSSRCBlockIndex(uint32_t inSSRC)
{
	uint32_t *blockBuffer = nullptr;
	int32_t count = 0;
	uint32_t ssrc = 0;

	if (nullptr == fNaduDataBuffer)
		return -1;

	for (; count < fNumBlocks; count++)
	{
		blockBuffer = fNaduDataBuffer + (count * 3);
		ssrc = (uint32_t)ntohl(*(uint32_t*)&blockBuffer[kOffsetNaduSSRC]);

		if (ssrc == inSSRC)
			return count;

	}

	return -1;
}

uint32_t RTCPNaduPacket::GetSSRC(int32_t index)
{

	if (index < 0)
		return 0;

	if (nullptr == fNaduDataBuffer)
		return 0;

	if (index >= fNumBlocks)
		return 0;

	uint32_t *blockBufferPtr = fNaduDataBuffer + (index * 3);
	auto ssrc = (uint32_t)ntohl(*(uint32_t*)&blockBufferPtr[kOffsetNaduSSRC]);

	return ssrc;

}

uint16_t RTCPNaduPacket::GetPlayOutDelay(int32_t index)
{
	if (index < 0)
		return 0;

	if (nullptr == fNaduDataBuffer)
		return 0;

	if (index >= fNumBlocks)
		return 0;

	uint32_t *blockBufferPtr = fNaduDataBuffer + (index * 3);
	auto delay = (uint16_t)((ntohl(*(uint32_t*)&blockBufferPtr[kOffsetNaduPlayoutDelay])  & kPlayoutMask) >> 16);

	return delay;
}

uint16_t RTCPNaduPacket::GetNSN(int32_t index)
{
	if (index < 0)
		return 0;

	if (nullptr == fNaduDataBuffer)
		return 0;

	if (index >= fNumBlocks)
		return 0;

	uint32_t *blockBufferPtr = fNaduDataBuffer + (index * 3);
	auto nsn = (uint16_t)(ntohl(blockBufferPtr[kOffsetNSN]) & kNSNMask);

	return nsn;
}

uint16_t RTCPNaduPacket::GetNUN(int32_t index)
{
	if (index < 0)
		return 0;

	if (nullptr == fNaduDataBuffer)
		return 0;

	if (index >= fNumBlocks)
		return 0;

	uint32_t *blockBufferPtr = fNaduDataBuffer + (index * 3);
	auto nun = (uint16_t)((ntohl(blockBufferPtr[kOffsetNUN]) & kNUNMask) >> 16);

	return nun;
}

uint16_t RTCPNaduPacket::GetFBS(int32_t index)
{
	if (index < 0)
		return 0;

	if (nullptr == fNaduDataBuffer)
		return 0;

	if (index >= fNumBlocks)
		return 0;

	uint32_t *blockBufferPtr = fNaduDataBuffer + (index * 3);
	uint16_t fbs = (uint16_t)ntohl(blockBufferPtr[kOffsetFBS]) & kFBSMask;

	return fbs;
}

void   RTCPNaduPacket::Dump()
{
	this->DumpNaduPacket();

}

/* class NaduReport */
NaduReport::NaduReport(uint8_t* inPacketBuffer, uint32_t inPacketLength, uint32_t id)
{
	fPacketBuffer = new uint8_t[inPacketLength + 1];
	fPacketBuffer[inPacketLength] = 0;
	fLength = inPacketLength;
	::memcpy(fPacketBuffer, inPacketBuffer, inPacketLength);
	fNaduPacket.ParseAPPData(fPacketBuffer, inPacketLength);
	fid = id;
}



/* class NaduList */

void NaduList::Initialize(uint32_t listSize)
{

	fNaduReportList = new NaduReport *[listSize];
	::memset((void *)fNaduReportList, 0, sizeof(NaduReport*) * listSize); //initialize ptr array with 0.
	fListSize = listSize;

}

NaduReport* NaduList::GetReport(uint32_t id)
{

	if (nullptr == fNaduReportList)
		return nullptr;


	NaduReport *result = fNaduReportList[this->IDtoIndex(id)];
	if (result && result->getID() == id)
		return result;
	return nullptr;

}

uint32_t NaduList::GetReportIndex(uint32_t id)
{

	if (nullptr == fNaduReportList)
		return 0;

	uint32_t index = this->IDtoIndex(id);
	NaduReport *result = fNaduReportList[index];
	if (result && result->getID() == id)
		return index;
	return 0;

}

NaduReport* NaduList::GetLastReport()
{
	if (nullptr == fNaduReportList || fcurrentIndexCount == 0)
		return nullptr;

	uint32_t index = this->IDtoIndex(fcurrentIndexCount);
	return fNaduReportList[index];

}

NaduReport* NaduList::GetPreviousReport(NaduReport* theReport)
{
	if (nullptr == theReport)
		return nullptr;

	return this->GetReport(theReport->getID() - 1);

}


NaduReport* NaduList::GetNextReport(NaduReport* theReport)
{
	if (nullptr == theReport)
		return nullptr;

	return this->GetReport(theReport->getID() + 1);

}

NaduReport* NaduList::GetEarliestReport()
{

	if (fcurrentIndexCount > fListSize)
		return fNaduReportList[fcurrentIndexCount % fListSize];

	return  fNaduReportList[0];
}


bool NaduList::AddReport(uint8_t* inPacketBuffer, uint32_t inPacketLength, uint32_t *outID)
{
	if (nullptr == fNaduReportList)
		return false;

	uint32_t resultID = ++fcurrentIndexCount;
	uint32_t index = this->IDtoIndex(fcurrentIndexCount);

	if (fNaduReportList[index] != nullptr)
		delete fNaduReportList[index];

	fNaduReportList[index] = new NaduReport(inPacketBuffer, inPacketLength, resultID);

	if (outID)
		*outID = resultID;

	return true;

}



uint32_t NaduList::LastReportedFreeBuffSizeBytes()
{
	NaduReport* currentReportPtr = this->GetLastReport();
	if (nullptr == currentReportPtr)
		return 0;

	RTCPNaduPacket *theNADUPacketData = currentReportPtr->GetNaduPacket();
	if (nullptr == theNADUPacketData)
		return 0;

	return ((uint32_t)theNADUPacketData->GetFBS(0)) * 64; //64 byte blocks are in the report
}

uint32_t NaduList::LastReportedTimeDelayMilli()
{
	NaduReport* currentReportPtr = this->GetLastReport();
	if (nullptr == currentReportPtr)
		return 0;

	RTCPNaduPacket *theNADUPacketData = currentReportPtr->GetNaduPacket();
	if (nullptr == theNADUPacketData)
		return 0;

	return theNADUPacketData->GetPlayOutDelay(0);
}

uint16_t NaduList::GetLastReportedNSN()
{
	NaduReport* currentReportPtr = this->GetLastReport();
	if (nullptr == currentReportPtr)
		return 0;

	RTCPNaduPacket *theNADUPacketData = currentReportPtr->GetNaduPacket();
	if (nullptr == theNADUPacketData)
		return 0;

	return theNADUPacketData->GetNSN(0);
}

void NaduList::DumpList()
{

	printf("\nDumping Nadu List (list size = %"   _U32BITARG_   "  record count=%"   _U32BITARG_   ")\n", fListSize, fcurrentIndexCount);
	printf("-------------------------------------------------------------\n");
	NaduReport* lastReportPtr = this->GetLastReport();
	NaduReport* earliestReportPtr = this->GetEarliestReport();
	uint32_t thisID = 0;
	uint32_t stopID = 0;
	if (earliestReportPtr)
		stopID = earliestReportPtr->getID();

	while (lastReportPtr)
	{
		thisID = lastReportPtr->getID();
		printf("NADU Record: list_index = %"   _U32BITARG_   " list_recordID = %"   _U32BITARG_   "\n", this->GetReportIndex(thisID), thisID);
		lastReportPtr->GetNaduPacket()->Dump();
		if (thisID == stopID)
			break;

		thisID--;
		lastReportPtr = this->GetReport(thisID);
	}

}
