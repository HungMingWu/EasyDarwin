include_directories()
IF (MSVC)
set (SRC Windows/hiredis.c Windows/hiredis.h
     Windows/net.c Windows/net.h
     Windows/sds.c Windows/sds.h)
 ELSE()
set (SRC hiredis.c hiredis.h sds.c sds.h read.c read.h net.c net.h)
 ENDIF()
add_library (EasyRedisClient ${SRC})
