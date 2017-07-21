include_directories(../../CommonUtilitiesLib 
					../../RTSPUtilitiesLib
					../../HTTPUtilitiesLib
					../../Include
					../../EasyRedisClient
					../../EasyProtocol/Include
					../../EasyProtocol/jsoncpp/include
					../APICommonCode
					../RTPMetaInfoLib
					../Server.tproj
					../APIStubLib
					../PrefsSourceLib
					../RTCPUtilitiesLib
					../
					QTSSReflectorModule/
					)

add_subdirectory(QTSSAccessLogModule)
add_subdirectory(QTSSFlowControlModule)
add_subdirectory(QTSSReflectorModule)
add_subdirectory(QTSSAccessModule)
add_subdirectory(EasyCMSModule)
add_subdirectory(EasyRedisModule)
add_subdirectory(QTSSPOSIXFileSysModule)
add_library (APIModules ${SRC})