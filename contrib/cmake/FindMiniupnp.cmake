# - Find Miniupnp
# This module defines
# MINIUPNP_INCLUDE_DIR, where to find Miniupnp headers
# MINIUPNP_LIBRARY, Miniupnp libraries
# MINIUPNP_FOUND, If false, do not try to use Miniupnp

set(MINIUPNP_PREFIX "" CACHE PATH "path ")

find_path(MINIUPNP_INCLUDE_DIR miniupnpc/miniupnpc.h
        PATHS ${MINIUPNP_PREFIX}/include /usr/include /usr/local/include )

find_library(MINIUPNP_LIBRARY NAMES miniupnpc libminiupnpc
        PATHS ${MINIUPNP_PREFIX}/lib /usr/lib /usr/local/lib)

if(MINIUPNP_INCLUDE_DIR AND MINIUPNP_LIBRARY)
    get_filename_component(MINIUPNP_LIBRARY_DIR ${MINIUPNP_LIBRARY} PATH)
    set(MINIUPNP_FOUND TRUE)
endif()

if(MINIUPNP_FOUND)
    if(NOT Miniupnp_FIND_QUIETLY)
        MESSAGE(STATUS "Found Miniupnp: ${MINIUPNP_LIBRARY}")
    endif()
else()
    if(MINIUPNP_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find Miniupnp")
    endif()
endif()

mark_as_advanced(
        MINIUPNP_LIBRARY
        MINIUPNP_INCLUDE_DIR
)