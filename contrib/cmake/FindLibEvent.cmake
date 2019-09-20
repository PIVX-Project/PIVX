# - Find LibEvent (a cross event library)
# This module defines
# LIBEVENT_INCLUDE_DIR, where to find LibEvent headers
# LIBEVENT_LIB, LibEvent libraries
# LibEvent_FOUND, If false, do not try to use libevent

if($ENV{target} MATCHES "Windows")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/depends/$ENV{triple}")
        set(LIBEVENT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/depends/$ENV{triple}/include")
        set(LIBEVENT_LIB "${CMAKE_CURRENT_SOURCE_DIR}/depends/$ENV{triple}/lib/libevent.a")
        set(LIBEVENT_PTHREAD_LIB "${CMAKE_CURRENT_SOURCE_DIR}/depends/$ENV{triple}/lib/libevent.a")
    endif()
else()
    set(LibEvent_EXTRA_PREFIXES /usr/local /opt/local "$ENV{HOME}")
    foreach(prefix ${LibEvent_EXTRA_PREFIXES})
        list(APPEND LibEvent_INCLUDE_PATHS  "${prefix}/include")
        list(APPEND LibEvent_LIB_PATHS "${prefix}/lib")
    endforeach()

    find_path(LIBEVENT_INCLUDE_DIR event.h PATHS ${LibEvent_INCLUDE_PATHS})
    find_library(LIBEVENT_LIB NAMES event PATHS ${LibEvent_LIB_PATHS})
    find_library(LIBEVENT_PTHREAD_LIB NAMES event_pthreads PATHS ${LibEvent_LIB_PATHS})
endif()

if (LIBEVENT_LIB AND LIBEVENT_INCLUDE_DIR AND LIBEVENT_PTHREAD_LIB)
    set(LibEvent_FOUND TRUE)
    set(LIBEVENT_LIB ${LIBEVENT_LIB} ${LIBEVENT_PTHREAD_LIB})
else ()
    set(LibEvent_FOUND FALSE)
endif ()

if (LibEvent_FOUND)
    if (NOT LibEvent_FIND_QUIETLY)
        message(STATUS "Found libevent: ${LIBEVENT_LIB}")
    endif ()
else ()
    if (LibEvent_FIND_REQUIRED)
        message(FATAL_ERROR "Could NOT find libevent and libevent_pthread.")
    endif ()
    message(STATUS "libevent and libevent_pthread NOT found.")
endif ()

mark_as_advanced(
        LIBEVENT_LIB
        LIBEVENT_INCLUDE_DIR
)
