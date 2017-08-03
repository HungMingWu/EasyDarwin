include_directories(../EasyProtocol/Include ../EasyDarwin/APICommonCode ../EasyDarwin/APIStubLib 
                    ../RTSPUtilitiesLib ../HTTPUtilitiesLib 
		            ../EasyDarwin/RTPMetaInfoLib/ ../EasyProtocol/Include .)
set (SRC 
             getopt.c getopt.h 
			 OSThread.cpp OSThread.h
			 StrPtrLen.cpp StrPtrLen.h
			 StringParser.cpp StringParser.h
			 StringFormatter.cpp StringFormatter.h
			 ResizeableStringFormatter.cpp ResizeableStringFormatter.h
			 OSMutex.cpp OSMutex.h
			 OSQueue.cpp OSQueue.h
			 OSFileSource.cpp OSFileSource.h
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
			 ConfParser.cpp ConfParser.h
			 TimeoutTask.cpp TimeoutTask.h
			 OSCond.cpp OSCond.h
			 UDPSocketPool.cpp UDPSocketPool.h
			 UDPSocket.cpp UDPSocket.h
			 TCPSocket.cpp TCPSocket.h
			 TCPListenerSocket.cpp TCPListenerSocket.h
			 GetWord.c GetWord.h
			 OSHeap.cpp OSHeap.h
			 OSBufferPool.cpp OSBufferPool.h
			 base64.c base64.h
			 sdpCache.cpp sdpCache.h
			 md5.c md5.h
			 md5digest.cpp md5digest.h
			 QueryParamList.cpp QueryParamList.h
			 DateTranslator.cpp DateTranslator.h
			 StringTranslator.cpp StringTranslator.h
			 SDPUtils.cpp SDPUtils.h
			 Trim.c Trim.h
			 UserAgentParser.cpp UserAgentParser.h
			 ClientSocket.cpp ClientSocket.h
			 keyframecache.cpp keyframecache.h
			 Format.cpp Format.h)
IF (MSVC)
set (SRC ${SRC} win32ev.cpp CreateDump.cpp)
ELSE()
set (SRC ${SRC} epollEvent.cpp epollEvent.h easy_gettimeofday.cpp easy_gettimeofday.h)
ENDIF()

add_library(CommonUtilitiesLib ${SRC})
