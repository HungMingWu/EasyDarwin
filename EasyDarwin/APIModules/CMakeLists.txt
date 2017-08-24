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
add_subdirectory(QTSSReflectorModule)
add_library (APIModules ${SRC})
