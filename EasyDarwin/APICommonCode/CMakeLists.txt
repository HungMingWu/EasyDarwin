include_directories(../../CommonUtilitiesLib 
                    ../../RTSPUtilitiesLib
                    ../../HTTPUtilitiesLib
					../../Include
                    ../RTPMetaInfoLib ../RTCPUtilitiesLib 
					../Server.tproj)
add_library (APICommonCode QTSSRollingLog.h QTSSRollingLog.cpp
	         SourceInfo.cpp SourceInfo.h
	         SDPSourceInfo.cpp SDPSourceInfo.h)
