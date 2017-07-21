include_directories()
IF (MSVC)
set (SRC Windows/hiredis.c Windows/hiredis.h
     Windows/net.c Windows/net.h
	 Windows/sds.c Windows/sds.h)
ENDIF()
add_library (EasyRedisClient ${SRC})