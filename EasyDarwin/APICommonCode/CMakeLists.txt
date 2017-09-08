include_directories(../../CommonUtilitiesLib 
                    ../../RTSPUtilitiesLib
                    ../../HTTPUtilitiesLib
					../../Include
                    ../RTPMetaInfoLib ../RTCPUtilitiesLib 
					../Server.tproj)
add_library (APICommonCode
	         SDPSourceInfo.cpp SDPSourceInfo.h)
