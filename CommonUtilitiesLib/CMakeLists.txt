include_directories(../EasyDarwin/APICommonCode ../EasyDarwin/APIStubLib 
                    ../RTSPUtilitiesLib ../HTTPUtilitiesLib 
		            ../EasyDarwin/RTPMetaInfoLib/ ../EasyProtocol/Include .)


set (SRC 
			 OSThread.cpp OSThread.h
			 StrPtrLen.cpp StrPtrLen.h
			 StringParser.cpp StringParser.h
			 StringFormatter.cpp StringFormatter.h
			 ResizeableStringFormatter.cpp ResizeableStringFormatter.h
			 OSMutex.cpp OSMutex.h
			 OSQueue.cpp OSQueue.h
			 MyAssert.cpp MyAssert.h
			 Task.cpp Task.h
			 SocketUtils.cpp SocketUtils.h
			 UDPDemuxer.cpp UDPDemuxer.h
			 IdleTask.cpp IdleTask.h
			 EventContext.cpp EventContext.h
			 OSRef.cpp OSRef.h
			 atomic.cpp atomic.h
			 OS.cpp OS.h
			 OSMutexRW.cpp OSMutexRW.h
			 Socket.cpp Socket.h
			 TimeoutTask.cpp TimeoutTask.h
			 OSCond.cpp OSCond.h
			 UDPSocketPool.cpp UDPSocketPool.h
			 UDPSocket.cpp UDPSocket.h
			 TCPSocket.cpp TCPSocket.h
			 TCPListenerSocket.cpp TCPListenerSocket.h
			 OSHeap.cpp OSHeap.h
			 base64.cpp base64.h
			 sdpCache.cpp sdpCache.h
			 md5.c md5.h
			 md5digest.cpp md5digest.h
			 QueryParamList.cpp QueryParamList.h
			 DateTranslator.cpp DateTranslator.h
			 SDPUtils.cpp SDPUtils.h
			 keyframecache.cpp keyframecache.h
			 Attributes.h
			 uri/decode.h uri/encode.h)
IF (MSVC)
set (SRC ${SRC} win32ev.cpp CreateDump.cpp)
ELSE()
set (SRC ${SRC} epollEvent.cpp epollEvent.h easy_gettimeofday.cpp easy_gettimeofday.h)
ENDIF()

add_library(CommonUtilitiesLib ${SRC})
