include_directories(../CommonUtilitiesLib
                    ../RTSPUtilitiesLib
                    ../EasyDarwin/APIStubLib 
					../EasyProtocol/Include
					.)
add_library (HTTPUtilitiesLib 
             HTTPRequest.cpp HTTPRequest.h
			 HTTPProtocol.cpp HTTPProtocol.h
			 HTTPClientRequestStream.cpp HTTPClientRequestStream.h
			 HTTPClientResponseStream.cpp HTTPClientResponseStream.h)
