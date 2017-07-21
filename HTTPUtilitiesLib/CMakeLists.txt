include_directories(../CommonUtilitiesLib
					../RTSPUtilitiesLib
                    ../EasyDarwin/APIStubLib)
add_library (HTTPUtilitiesLib 
             HTTPRequest.cpp HTTPRequest.h
			 HTTPProtocol.cpp HTTPProtocol.h)