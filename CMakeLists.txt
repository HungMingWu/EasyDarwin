cmake_minimum_required (VERSION 3.0)
project (EasyDarwin C CXX)

find_package(Boost 1.61 REQUIRED COMPONENTS system)
find_package(fmt REQUIRED)

if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 
endif()

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
    add_definitions(-include "${CMAKE_CURRENT_SOURCE_DIR}/Include/PlatformHeader.h")
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++14")
    set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-O0 -ggdb -std=c99")
endif()

add_subdirectory (CommonUtilitiesLib)
add_subdirectory (RTSPUtilitiesLib)
add_subdirectory (EasyDarwin)