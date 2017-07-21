cmake_minimum_required (VERSION 3.0)
project (EasyDarwin)

IF (MSVC)
#add_definitions(-D__Win32__)
ENDIF ()

if(MSVC)
	#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "/FI ${CMAKE_CURRENT_SOURCE_DIR}/Include/Win32header.h")
	#set(MY_CXX_FLAGS "/FI ${CMAKE_CURRENT_SOURCE_DIR}/Include/Win32header.h")
	#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} ${MY_CXX_FLAGS})
	add_compile_options("/FI ${CMAKE_CURRENT_SOURCE_DIR}/Include/Win32header.h")
else()
    # GCC or Clang
    #add_definitions(-include foo.h)
endif()

add_subdirectory (CommonUtilitiesLib)
add_subdirectory (SafeStdLib)
add_subdirectory (RTSPUtilitiesLib)
add_subdirectory (HTTPUtilitiesLib)
add_subdirectory (EasyProtocol)
add_subdirectory (EasyRedisClient)
add_subdirectory (EasyDarwin)