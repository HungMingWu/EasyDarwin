include_directories(../../CommonUtilitiesLib ../../RTSPUtilitiesLib 
					../../HTTPUtilitiesLib ../../Include
					../APIModules/QTSSReflectorModule 
					../RTCPUtilitiesLib ../APICommonCode
					../RTPMetaInfoLib
					../
					./)

set (SRC 
     RunServer.cpp RunServer.h
	 QTSServer.cpp QTSServer.h
	 QTSServerInterface.cpp QTSServerInterface.h
     RTSPRequestInterface.cpp RTSPRequestInterface.h
     RTPSession.cpp RTPSession.h
	 MyRTPSession.cpp MyRTPSession.h
     RTPStream.cpp RTPStream.h
	 MyRTPStream.cpp MyRTPStream.h
     RTSPSessionInterface.cpp RTSPSessionInterface.h
     RTPSessionInterface.cpp RTPSessionInterface.h
     RTSPSession.cpp RTSPSession.h
	 MyRTSPSession.cpp MyRTSPSession.h
     RTSPResponseStream.cpp RTSPResponseStream.h
     RTSPRequest.cpp RTSPRequest.h
	 MyRTSPRequest.cpp MyRTSPRequest.h
	 RTSPRequestStream.cpp RTSPRequestStream.h
	 ServerPrefs.h ServerPrefs.cpp
	 RTSPServer.h RTSPServer.cpp
	 coroutine_wrappers.h coroutine_wrappers.cpp
	 Uri.h)

link_libraries(APIModules RTCPUtilitiesLib RTSPUtilitiesLib
               APICommonCode
               CommonUtilitiesLib)
IF (MSVC)
link_libraries(Ws2_32 Winmm)
set (SRC ${SRC} win32main.cpp)
ELSE()
	link_libraries(pthread dl crypt)
	set (SRC ${SRC} main.cpp)
ENDIF()
add_executable (EasyDarwin ${SRC})
TARGET_LINK_LIBRARIES(EasyDarwin  ${Boost_SYSTEM_LIBRARY} fmt::fmt)
