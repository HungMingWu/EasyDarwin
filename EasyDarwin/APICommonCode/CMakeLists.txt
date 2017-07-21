include_directories(../../CommonUtilitiesLib 
                    ../../RTSPUtilitiesLib
					../../HTTPUtilitiesLib
                    ../RTPMetaInfoLib ../APIStubLib)
add_library (APICommonCode QTSSRollingLog.h QTSSRollingLog.cpp
             QTSSModuleUtils.cpp QTSSModuleUtils.h
			 SourceInfo.cpp SourceInfo.h)