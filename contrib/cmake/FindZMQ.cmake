# - Find ZeroMQ
# This module defines
# ZMQ_INCLUDE_DIR, where to find ZMQ headers
# ZMQ_LIB, ZMQ libraries
# ZMQ_FOUND, If false, do not try to use ZeroMQ

set(ZMQ_EXTRA_PREFIXES /usr/local /opt/local "$ENV{HOME}")
foreach(prefix ${ZMQ_EXTRA_PREFIXES})
    list(APPEND ZMQ_INCLUDE_PATHS "${prefix}/include")
    list(APPEND ZMQ_LIB_PATHS "${prefix}/lib")
endforeach()

find_path(ZMQ_INCLUDE_DIR zmq.h PATHS ${ZMQ_INCLUDE_PATHS})
find_library(ZMQ_LIB NAMES zmq PATHS ${ZMQ_LIB_PATHS})

if (ZMQ_LIB AND ZMQ_INCLUDE_DIR)
    set(ZMQ_FOUND TRUE)
else ()
    set(ZMQ_FOUND FALSE)
endif ()

if (ZMQ_FOUND)
    if (NOT ZMQ_FIND_QUIETLY)
        message(STATUS "Found ZeroMQ: ${ZMQ_LIB}")
        include_directories(${ZMQ_INCLUDE_DIR})
    endif ()
else ()
    if (ZMQ_FIND_REQUIRED)
        message(FATAL_ERROR "Could NOT find ZeroMQ.")
    endif ()
    message(STATUS "ZeroMQ NOT found.")
endif ()

mark_as_advanced(
        ZMQ_LIB
        ZMQ_INCLUDE_DIR
)
