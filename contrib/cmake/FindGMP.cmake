# - Find GMP
# This module defines
# GMP_INCLUDE_DIR, where to find GMP headers
# GMP_LIBRARY, LibEvent libraries
# GMP_FOUND, If false, do not try to use GMP

set(GMP_PREFIX "" CACHE PATH "path ")

find_path(GMP_INCLUDE_DIR gmp.h gmpxx.h
        PATHS ${GMP_PREFIX}/include /usr/include /usr/local/include )

find_library(GMP_LIBRARY NAMES gmp libgmp
        PATHS ${GMP_PREFIX}/lib /usr/lib /usr/local/lib)

if(GMP_INCLUDE_DIR AND GMP_LIBRARY)
    get_filename_component(GMP_LIBRARY_DIR ${GMP_LIBRARY} PATH)
    set(GMP_FOUND TRUE)
endif()

if(GMP_FOUND)
    if(NOT GMP_FIND_QUIETLY)
        MESSAGE(STATUS "Found GMP: ${GMP_LIBRARY}")
    endif()
elseif(GMP_FOUND)
    if(GMP_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find GMP")
    endif()
endif()

mark_as_advanced(
        GMP_LIB
        GMP_INCLUDE_DIR
)
