include_directories(../../CommonUtilitiesLib 
                    ../../RTSPUtilitiesLib
                    ../../HTTPUtilitiesLib
					../../Include
                    ../RTPMetaInfoLib ../RTCPUtilitiesLib 
					../Server.tproj)
add_library (APICommonCode
	         SourceInfo.cpp SourceInfo.h
	         SDPSourceInfo.cpp SDPSourceInfo.h)
