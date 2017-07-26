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
					../QTFileLib
					../
					QTSSReflectorModule/
					)

# Common module
add_subdirectory(EasyCMSModule)
add_subdirectory(EasyRedisModule)
add_subdirectory(QTSSAccessLogModule)
add_subdirectory(QTSSAccessModule)
add_subdirectory(QTSSFlowControlModule)
add_subdirectory(QTSSPOSIXFileSysModule)
add_subdirectory(QTSSReflectorModule)
add_subdirectory(QTSSAdminModule)
add_subdirectory(QTSSWebDebugModule)
add_library (APIModules ${SRC})
