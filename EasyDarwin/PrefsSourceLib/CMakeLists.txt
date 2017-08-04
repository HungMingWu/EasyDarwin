include_directories(../../CommonUtilitiesLib ../../EasyProtocol/Include)
add_library (PrefsSourceLib FilePrefsSource.h FilePrefsSource.cpp
             XMLParser.cpp XMLParser.h
             XMLPrefsParser.cpp XMLPrefsParser.h)
