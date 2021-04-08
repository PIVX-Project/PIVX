# - Find Qrcode
# This module defines
# QRCODE_INCLUDE_DIR, where to find libqrencode headers
# QRCODE_LIB, libqrencode libraries
# QRCODE_FOUND, If false, do not try to use libqrencode

set(QRCODE_EXTRA_PREFIXES /usr/local /opt/local "$ENV{HOME}")
foreach(prefix ${ZMQ_EXTRA_PREFIXES})
    list(APPEND QRCODE_INCLUDE_PATHS "${prefix}/include")
    list(APPEND QRCODE_LIB_PATHS "${prefix}/lib")
endforeach()

find_path(QRCODE_INCLUDE_DIR qrencode.h PATHS ${QRCODE_INCLUDE_PATHS})
find_library(QRCODE_LIB NAMES qrencode PATHS ${QRCODE_LIB_PATHS})

if (QRCODE_LIB AND QRCODE_INCLUDE_DIR)
    set(QRCODE_FOUND TRUE)
else ()
    set(QRCODE_FOUND FALSE)
endif ()

if (QRCODE_FOUND)
    if (NOT QRCODE_FIND_QUIETLY)
        message(STATUS "Found libqrencode: ${QRCODE_LIB}")
        include_directories(${QRCODE_INCLUDE_DIR})
    endif ()
else ()
    if (QRCODE_FIND_REQUIRED)
        message(FATAL_ERROR "Could NOT find libqrencode.")
    endif ()
    message(STATUS "libqrencode NOT found.")
endif ()

mark_as_advanced(
        QRCODE_LIB
        QRCODE_INCLUDE_DIR
)
