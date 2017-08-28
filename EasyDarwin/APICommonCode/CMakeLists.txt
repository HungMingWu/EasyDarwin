include_directories(../../CommonUtilitiesLib 
                    ../../RTSPUtilitiesLib
                    ../../HTTPUtilitiesLib
					../../Include
                    ../RTPMetaInfoLib ../RTCPUtilitiesLib 
					../PrefsSourceLib ../Server.tproj)
add_library (APICommonCode QTSSRollingLog.h QTSSRollingLog.cpp
             QTSSModuleUtils.cpp QTSSModuleUtils.h
	         SourceInfo.cpp SourceInfo.h
	         SDPSourceInfo.cpp SDPSourceInfo.h)
