include_directories(../../CommonUtilitiesLib 
					../../RTSPUtilitiesLib
					../../Include
					../APICommonCode
					../RTPMetaInfoLib
					../Server.tproj
					../APIStubLib
					../RTCPUtilitiesLib
					../
					QTSSReflectorModule/
					)

# Common module
add_subdirectory(QTSSReflectorModule)
add_library (APIModules ${SRC})
