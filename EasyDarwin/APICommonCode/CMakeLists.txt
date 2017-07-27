include_directories(../../CommonUtilitiesLib 
                    ../../RTSPUtilitiesLib
                    ../../HTTPUtilitiesLib
					../../EasyProtocol/Include
                    ../RTPMetaInfoLib ../RTCPUtilitiesLib 
					../PrefsSourceLib ../APIStubLib ../Server.tproj)
add_library (APICommonCode QTSSRollingLog.h QTSSRollingLog.cpp
             QTSSModuleUtils.cpp QTSSModuleUtils.h
	     SourceInfo.cpp SourceInfo.h
	     QTAccessFile.cpp QTAccessFile.h
	     SDPSourceInfo.cpp SDPSourceInfo.h)
