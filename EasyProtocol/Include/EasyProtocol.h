/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
#ifndef EASY_PROTOCOL_H
#define	EASY_PROTOCOL_H

#include <EasyProtocolBase.h>
#include <unordered_map>
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <list>

using namespace std;

namespace EasyDarwin { namespace Protocol
{

	class Easy_API EasyDevice//摄像头信息类
	{
	public:
		EasyDevice();
		EasyDevice(const string& serial, const string& name);
		EasyDevice(const string& serial, const string& name, const string& status);
		~EasyDevice() = default;

	public:
		string serial_;
		string name_;
		string status_;	//online/offline
		string password_;
		string tag_;
		string channel_;
		string snapJpgPath_;//最新的快照路径
	};

	typedef unordered_map<string, EasyDevice> EasyDevices;		//摄像头表，改为map.方便查找。key为channel_，value为EasyDevice

	typedef boost::variant<int, float, string> value_t;
	typedef unordered_map<string, value_t> EasyJsonValue;	//key为string,value可以是int、float、string的一种
	typedef void* EasyObject;

	class EasyJsonValueVisitor : public boost::static_visitor<string>
	{
	public:
		string operator()(int value) const { return boost::lexical_cast<string>(value); }
		string operator()(float value) const { return boost::lexical_cast<string>(value); }
		string operator()(string value) const { return value; }
	};

	class Easy_API EasyNVR : public EasyDevice//NVR类型
	{
	public:
		EasyNVR();
		EasyNVR(const string& serial, const string& name, const string& password, const string& tag, EasyDevices &channel);
		~EasyNVR() = default;

	public:
		EasyDevices channels_;
		EasyObject object_{nullptr};
	};

	// MSG_DS_REGISTER_REQ
	class Easy_API EasyMsgDSRegisterREQ : public EasyProtocol	//封装NVR的注册请求
	{
	public:
		EasyMsgDSRegisterREQ(EasyDarwinTerminalType terminalType, EasyDarwinAppType appType, EasyNVR &nvr, size_t cseq = 1);
		EasyMsgDSRegisterREQ(const string& msg);
		~EasyMsgDSRegisterREQ() override = default;

	public:
		EasyNVR& GetNVR() { return nvr_; }

	private:
		EasyNVR nvr_;
	};

	// MSG_SD_REGISTER_ACK
	class Easy_API EasyMsgSDRegisterACK : public EasyProtocol	//封装NVR的注册回应
	{
	public:
		EasyMsgSDRegisterACK(EasyJsonValue &body, size_t cseq = 1, size_t error = 200);
		EasyMsgSDRegisterACK(const string& msg);
		~EasyMsgSDRegisterACK() override = default;
	};

	// MSG_SD_PUSH_STREAM_REQ
	class EasyMsgSDPushStreamREQ : public EasyProtocol
	{
	public:
		EasyMsgSDPushStreamREQ(EasyJsonValue &body, size_t cseq);
		EasyMsgSDPushStreamREQ(const string& msg);
		~EasyMsgSDPushStreamREQ() override = default;
	};

	// MSG_DS_PUSH_STREAM_ACK
	class EasyMsgDSPushSteamACK : public EasyProtocol
	{
	public:
		EasyMsgDSPushSteamACK(EasyJsonValue &body, size_t cseq = 1, size_t error = 200);
		EasyMsgDSPushSteamACK(const string& msg);
		~EasyMsgDSPushSteamACK() override = default;
	};

	// MSG_SD_STREAM_STOP_REQ
	class EasyMsgSDStopStreamREQ : public EasyProtocol
	{
	public:
		EasyMsgSDStopStreamREQ(EasyJsonValue &body, size_t cseq = 1);
		EasyMsgSDStopStreamREQ(const string& msg);
		~EasyMsgSDStopStreamREQ() override = default;
	};

	// MSG_DS_STREAM_STOP_ACK
	class EasyMsgDSStopStreamACK : public EasyProtocol
	{
	public:
		EasyMsgDSStopStreamACK(EasyJsonValue &body, size_t cseq = 1, size_t error = 200);
		EasyMsgDSStopStreamACK(const string& msg);
		~EasyMsgDSStopStreamACK() override = default;
	};

	// MSG_SC_DEVICE_LIST_ACK
	class EasyMsgSCDeviceListACK : public EasyProtocol
	{
	public:
		EasyMsgSCDeviceListACK(EasyDevices &devices, size_t cseq = 1, size_t error = 200);
		EasyMsgSCDeviceListACK(const string& msg);
		~EasyMsgSCDeviceListACK() override = default;

		EasyDevices& GetDevices() { return devices_; }

	private:
		EasyDevices devices_;
	};

	// MSG_SC_DEVICE_INFO_ACK
	class EasyMsgSCDeviceInfoACK : public EasyProtocol
	{
	public:
		EasyMsgSCDeviceInfoACK(EasyDevices &cameras, const string& devcei_serial, size_t cseq = 1, size_t error = 200);
		EasyMsgSCDeviceInfoACK(const string& msg);
		~EasyMsgSCDeviceInfoACK() override = default;

		EasyDevices& GetCameras() { return channels_; }

	private:
		EasyDevices channels_;
	};

	// MSG_SC_GET_STREAM_ACK
	class EasyMsgSCGetStreamACK : public EasyProtocol
	{
	public:
		EasyMsgSCGetStreamACK(EasyJsonValue &body, size_t cseq = 1, size_t error = 200);
		EasyMsgSCGetStreamACK(const string& msg);
		~EasyMsgSCGetStreamACK() override = default;
	};

	// MSG_CS_FREE_STREAM_REQ
	class EasyMsgCSFreeStreamREQ : public EasyProtocol
	{
	public:
		EasyMsgCSFreeStreamREQ(EasyJsonValue &body, size_t cseq);
		EasyMsgCSFreeStreamREQ(const string& msg);
		~EasyMsgCSFreeStreamREQ() override = default;
	};

	// MSG_SC_FREE_STREAM_ACK
	class EasyMsgSCFreeStreamACK : public EasyProtocol
	{
	public:
		EasyMsgSCFreeStreamACK(EasyJsonValue &body, size_t cseq = 1, size_t error = 200);
		EasyMsgSCFreeStreamACK(const string& msg);
		~EasyMsgSCFreeStreamACK() override = default;
	};

	// MSG_DS_POST_SNAP_REQ
	class EasyMsgDSPostSnapREQ : public EasyProtocol
	{
	public:
		EasyMsgDSPostSnapREQ(EasyJsonValue &body, size_t cseq = 1);
		EasyMsgDSPostSnapREQ(const string& msg);
		~EasyMsgDSPostSnapREQ() override = default;
	};

	// MSG_SD_POST_SNAP_ACK
	class EasyMsgSDPostSnapACK : public EasyProtocol
	{
	public:
		EasyMsgSDPostSnapACK(EasyJsonValue &body, size_t cseq = 1, size_t error = 200);
		EasyMsgSDPostSnapACK(const string& msg);
		~EasyMsgSDPostSnapACK() override = default;
	};

	// MSG_CS_PTZ_CONTROL_REQ
	class EasyMsgCSPTZControlREQ : public EasyProtocol
	{
	public:
		EasyMsgCSPTZControlREQ(EasyJsonValue& body, size_t cseq = 1);
		EasyMsgCSPTZControlREQ(const string& msg);
		~EasyMsgCSPTZControlREQ() override = default;
	};

	// MSG_SC_PTZ_CONTROL_ACK
	class EasyMsgSCPTZControlACK : public EasyProtocol
	{
	public:
		EasyMsgSCPTZControlACK(EasyJsonValue& body, size_t cseq = 1, size_t error = 200);
		EasyMsgSCPTZControlACK(const string& msg);
		~EasyMsgSCPTZControlACK() override = default;
	};

	// MSG_SD_CONTROL_PTZ_REQ
	class EasyMsgSDControlPTZREQ : public EasyProtocol
	{
	public:
		EasyMsgSDControlPTZREQ(EasyJsonValue& body, size_t cseq = 1);
		EasyMsgSDControlPTZREQ(const string& msg);
		~EasyMsgSDControlPTZREQ() override = default;
	};

	// MSG_DS_CONTROL_PTZ_ACK
	class EasyMsgDSControlPTZACK : public EasyProtocol
	{
	public:
		EasyMsgDSControlPTZACK(EasyJsonValue& body, size_t cseq = 1, size_t error = 200);
		EasyMsgDSControlPTZACK(const string& msg);
		~EasyMsgDSControlPTZACK() override = default;
	};

	// MSG_CS_PRESET_CONTROL_REQ
	class EasyMsgCSPresetControlREQ : public EasyProtocol
	{
	public:
		EasyMsgCSPresetControlREQ(EasyJsonValue& body, size_t cseq = 1);
		EasyMsgCSPresetControlREQ(const string& msg);
		~EasyMsgCSPresetControlREQ() override = default;
	};

	// MSG_SC_PRESET_CONTROL_ACK
	class EasyMsgSCPresetControlACK : public EasyProtocol
	{
	public:
		EasyMsgSCPresetControlACK(EasyJsonValue& body, size_t cseq = 1, size_t error = 200);
		EasyMsgSCPresetControlACK(const string& msg);
		~EasyMsgSCPresetControlACK() override = default;
	};

	// MSG_SD_CONTROL_PRESET_REQ
	class EasyMsgSDControlPresetREQ : public EasyProtocol
	{
	public:
		EasyMsgSDControlPresetREQ(EasyJsonValue& body, size_t cseq = 1);
		EasyMsgSDControlPresetREQ(const string& msg);
		~EasyMsgSDControlPresetREQ() override = default;
	};

	// MSG_DS_CONTROL_PRESET_ACK
	class EasyMsgDSControlPresetACK : public EasyProtocol
	{
	public:
		EasyMsgDSControlPresetACK(EasyJsonValue& body, size_t cseq = 1, size_t error = 200);
		EasyMsgDSControlPresetACK(const string& msg);
		~EasyMsgDSControlPresetACK() override = default;
	};

	// MSG_CS_TALKBACK_CONTROL_REQ
	class EasyMsgCSTalkbackControlREQ : public EasyProtocol
	{
	public:
		EasyMsgCSTalkbackControlREQ(EasyJsonValue& body, size_t cseq = 1);
		EasyMsgCSTalkbackControlREQ(const string& msg);
		~EasyMsgCSTalkbackControlREQ() override = default;
	};

	// MSG_SC_TALKBACK_CONTROL_ACK
	class EasyMsgSCTalkbackControlACK : public EasyProtocol
	{
	public:
		EasyMsgSCTalkbackControlACK(EasyJsonValue& body, size_t cseq = 1, size_t error = 200);
		EasyMsgSCTalkbackControlACK(const string& msg);
		~EasyMsgSCTalkbackControlACK() override = default;
	};

	// MSG_SD_CONTROL_TALKBACK_REQ
	class EasyMsgSDControlTalkbackREQ : public EasyProtocol
	{
	public:
		EasyMsgSDControlTalkbackREQ(EasyJsonValue& body, size_t cseq = 1);
		EasyMsgSDControlTalkbackREQ(const string& msg);
		~EasyMsgSDControlTalkbackREQ() override = default;
	};

	// MSG_DS_CONTROL_TALKBACK_ACK
	class EasyMsgDSControlTalkbackACK : public EasyProtocol
	{
	public:
		EasyMsgDSControlTalkbackACK(EasyJsonValue& body, size_t cseq = 1, size_t error = 200);
		EasyMsgDSControlTalkbackACK(const string& msg);
		~EasyMsgDSControlTalkbackACK() override = default;
	};

	class EasyMsgExceptionACK : public EasyProtocol
	{
	public:
		EasyMsgExceptionACK(size_t cseq = 1, size_t error = 400);
		~EasyMsgExceptionACK() override = default;
	};

	class EasyDarwinHLSession
	{
	public:
		EasyDarwinHLSession() 
		{
		}

		~EasyDarwinHLSession() = default;

	public:
		int index{0};
		std::string SessionName;
		std::string HlsUrl;
		std::string sourceUrl;
		int bitrate{0};
	};

	class EasyDarwinRTSPSession
	{
	public:
		EasyDarwinRTSPSession()
		{
		}

		~EasyDarwinRTSPSession() = default;

	public:
		int index{0};
		std::string Url;
		std::string Name;
		int channel{0};
		int numOutputs{0};
	};

	// MSG_SC_START_HLS_ACK
	class Easy_API EasyMsgSCStartHLSACK : public EasyProtocol
	{
	public:
		EasyMsgSCStartHLSACK();
		EasyMsgSCStartHLSACK(const string& msg);
		~EasyMsgSCStartHLSACK() override = default;

		void SetStreamName(const string& sName);
		void SetStreamURL(const string& sURL);
	};

	// MSG_SC_HLS_SESSION_LIST_ACK
	class Easy_API EasyMsgSCHLSessionListACK : public EasyProtocol
	{
	public:
		EasyMsgSCHLSessionListACK();
		EasyMsgSCHLSessionListACK(const string& msg);
		~EasyMsgSCHLSessionListACK() override = default;

	public:
		bool AddSession(EasyDarwinHLSession &session);
		//int StartGetDevice();
		//bool GetNextDevice(EasyDarwinDevice &device);

	private:
		list<EasyDarwinHLSession> sessions;
	};

	// MSG_SC_RTSP_LIVE_SESSIONS_ACK
	class Easy_API EasyMsgSCRTSPLiveSessionsACK : public EasyProtocol
	{
	public:
		EasyMsgSCRTSPLiveSessionsACK();
		EasyMsgSCRTSPLiveSessionsACK(const string& msg);
		~EasyMsgSCRTSPLiveSessionsACK() override = default;

	public:
		bool AddSession(EasyDarwinRTSPSession& session);
		//int StartGetDevice();
		//bool GetNextDevice(EasyDarwinDevice &device);

	private:
		std::list<EasyDarwinRTSPSession> sessions;
	};

	// MSG_SC_LIST_RECORD_ACK
	class Easy_API EasyMsgSCListRecordACK : public EasyProtocol
	{
	public:
		EasyMsgSCListRecordACK();
		EasyMsgSCListRecordACK(const string& msg);
		~EasyMsgSCListRecordACK() override = default;

	public:
		bool AddRecord(const string& record);

	private:
		std::list<std::string> records;
	};

	class Easy_API EasyProtocolACK : public EasyProtocol
	{
	public:
		EasyProtocolACK(int iMsgType) :EasyProtocol(iMsgType) {}
		void SetHead(EasyJsonValue& header);//设置头部
		void SetBody(EasyJsonValue& body);//设置JSON
	};

	class Easy_API EasyMsgSCRecordListACK : public EasyProtocolACK//封装录像列表回应
	{
	public:
		EasyMsgSCRecordListACK(int iMsgType) :EasyProtocolACK(iMsgType) {}
		void AddRecord(const string& record);
		void AddRecord(int day_of_month/*1~31*/);
		void AddRecord(const string& starttime, const string &endtime);//format-HH:MM:SS
	};


	class EasyDarwinRecordSession
	{
	public:
		EasyDarwinRecordSession() 
		{
		}

		~EasyDarwinRecordSession() = default;

	public:
		int index{0};
		long long startTime{0};
		long long endTime{0};
		std::string Name;
		int numOutputs{0};
	};

	class Easy_API EasyMsgSCRecordList : public EasyProtocol//封装录像列表回应
	{

	public:
		EasyMsgSCRecordList();
		EasyMsgSCRecordList(const string& msg);
		~EasyMsgSCRecordList() override = default;

	public:
		bool AddRecord(EasyDarwinRecordSession& session);
		//int StartGetDevice();
		//bool GetNextDevice(EasyDarwinDevice &device);

	private:
		std::list<EasyDarwinRecordSession> sessions;
	};

	//add,Unisiot，start
	typedef struct
	{
		string strDeviceSerial;//设备序列号
		string strCameraSerial;//摄像头序列号
		string strProtocol;//直播协议
		string strStreamID;//直播流类型
	} stStreamInfo;

	class strDevice//设备类型对应的信息
	{
	public:
		strDevice();
		bool GetDevInfo(const string& msg);//由JSON文本得到设备信息
		void HoldSnapPath(const string& strJpgPath, const string& strChannel);//保留快照的路径
	public:
		string serial_;//设备序列号
		string name_;//设备名称
		string password_;//密码
		string tag_;//标签
		string channelCount_;//该设备包含的摄像头个数
		string snapJpgPath_;//最新的快照路径


		EasyDevices channels_;//摄像头信息
		EasyDarwinTerminalType eDeviceType{};//设备类型
		EasyDarwinAppType eAppType{};//App类型
	};
//add,Unisiot，end
}}//namespace
#endif
