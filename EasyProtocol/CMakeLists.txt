include_directories(Include/ jsoncpp/include)
add_library (EasyProtocol 
             EasyProtocol/EasyProtocol.cpp
	         EasyProtocol/EasyProtocolBase.cpp
			 EasyProtocol/EasyUtil.cpp)

add_subdirectory (jsoncpp)