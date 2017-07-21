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
     QTSSSocket.cpp QTSSSocket.h
     RTSPSession.cpp RTSPSession.h
     RTPPacketResender.cpp RTPPacketResender.h
     RTPBandwidthTracker.cpp RTPBandwidthTracker.h
     QTSSUserProfile.cpp QTSSUserProfile.h
     RTSPResponseStream.cpp RTSPResponseStream.h
     RTSPRequest.cpp RTSPRequest.h
	 QTSSExpirationDate.cpp QTSSExpirationDate.h
	 RTSPRequestStream.cpp RTSPRequestStream.h
	 HTTPSessionInterface.cpp HTTPSessionInterface.h
	 HTTPSession.cpp HTTPSession.h
	 GenerateXMLPrefs.cpp GenerateXMLPrefs.h
	 QTSSErrorLogModule.cpp QTSSErrorLogModule.h
	 RTCPTask.cpp RTCPTask.h)

IF (MSVC)
link_libraries(Ws2_32 Winmm)
set (SRC ${SRC} win32main.cpp)
ENDIF()

link_libraries(HTTPUtilitiesLib RTCPUtilitiesLib RTSPUtilitiesLib PrefsSourceLib
               APICommonCode APIModules APIStubLib SafeStdLib EasyRedisClient
			   EasyProtocol CommonUtilitiesLib RTPMetaInfoLib jsoncpp)
add_executable (EasyDarwin ${SRC})