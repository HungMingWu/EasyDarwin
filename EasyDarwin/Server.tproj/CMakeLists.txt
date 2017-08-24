include_directories(../../CommonUtilitiesLib ../../RTSPUtilitiesLib 
					../../HTTPUtilitiesLib ../../Include
					../../EasyProtocol/Include ../../EasyProtocol/jsoncpp/include
					../APIModules/QTSSReflectorModule 
					../APIModules/EasyCMSModule
					../APIModules/QTSSAccessLogModule
					../APIModules/QTSSFlowControlModule
					../APIModules/EasyCMSModule 
					../APIModules/QTSSPOSIXFileSysModule
					../APIModules/EasyRedisModule
					../APIModules/QTSSAccessModule
					../PrefsSourceLib ../APIStubLib ../RTCPUtilitiesLib ../APICommonCode
					../RTPMetaInfoLib
					../)

set (SRC 
     RunServer.cpp RunServer.h
	 QTSSCallbacks.cpp QTSSCallbacks.h
	 QTSSModule.cpp QTSSModule.h
	 QTSServer.cpp QTSServer.h
     QTSSDictionary.cpp QTSSDictionary.h
	 QTSServerInterface.cpp QTSServerInterface.h
     RTSPRequestInterface.cpp RTSPRequestInterface.h
     QTSServerPrefs.cpp QTSServerPrefs.h
     QTSSDataConverter.cpp QTSSDataConverter.h
     RTPSession.cpp RTPSession.h
     RTPStream.cpp RTPStream.h
     RTPOverbufferWindow.cpp RTPOverbufferWindow.h
     RTSPSessionInterface.cpp RTSPSessionInterface.h
     QTSSPrefs.cpp QTSSPrefs.h
     QTSSFile.cpp QTSSFile.h
     RTPSessionInterface.cpp RTPSessionInterface.h
     QTSSMessages.cpp QTSSMessages.h
     RTSPSession.cpp RTSPSession.h
     RTPPacketResender.cpp RTPPacketResender.h
     RTPBandwidthTracker.cpp RTPBandwidthTracker.h
     QTSSUserProfile.cpp QTSSUserProfile.h
     RTSPResponseStream.cpp RTSPResponseStream.h
     RTSPRequest.cpp RTSPRequest.h
	 QTSSExpirationDate.cpp QTSSExpirationDate.h
	 RTSPRequestStream.cpp RTSPRequestStream.h
	 GenerateXMLPrefs.cpp GenerateXMLPrefs.h
	 QTSSErrorLogModule.cpp QTSSErrorLogModule.h
	 RTCPTask.cpp RTCPTask.h)

link_libraries(APIModules RTCPUtilitiesLib RTSPUtilitiesLib PrefsSourceLib
               APICommonCode APIStubLib
               CommonUtilitiesLib RTPMetaInfoLib)
IF (MSVC)
link_libraries(Ws2_32 Winmm)
set (SRC ${SRC} win32main.cpp)
ELSE()
	link_libraries(pthread dl crypt)
	set (SRC ${SRC} main.cpp)
ENDIF()
add_executable (EasyDarwin ${SRC})
TARGET_LINK_LIBRARIES(EasyDarwin  ${Boost_SYSTEM_LIBRARY} fmt::fmt)