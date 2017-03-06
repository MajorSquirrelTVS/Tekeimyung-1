# Find FMOD
#
# Below are the output variables:
#  - SQUIRREL_ENGINE_INCLUDE_DIR
#  - SQUIRREL_ENGINE_LIBRARY
#  - SQUIRREL_ENGINE_FOUND

find_path(SQUIRREL_ENGINE_INCLUDE_DIR
    NAMES Core/Engine.hpp
    PATHS $ENV{SQUIRREL_ENGINE_ROOT}/include ${SQUIRREL_ENGINE_ROOT}/include
)

find_library(SQUIRREL_ENGINE_LIBRARY_RELEASE
    NAMES squirrel_engine
    PATHS $ENV{SQUIRREL_ENGINE_ROOT} ${SQUIRREL_ENGINE_ROOT}
    PATH_SUFFIXES lib
)

find_library(SQUIRREL_ENGINE_LIBRARY_DEBUG
    NAMES squirrel_engine
    PATHS $ENV{SQUIRREL_ENGINE_ROOT} ${SQUIRREL_ENGINE_ROOT}
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQUIRREL_ENGINE DEFAULT_MSG SQUIRREL_ENGINE_LIBRARY_RELEASE SQUIRREL_ENGINE_INCLUDE_DIR)

if(SQUIRREL_ENGINE_FOUND)
    set(SQUIRREL_ENGINE_LIBRARY debug ${SQUIRREL_ENGINE_LIBRARY_DEBUG}
                       optimized ${SQUIRREL_ENGINE_LIBRARY_RELEASE}
    )
endif()

mark_as_advanced(SQUIRREL_ENGINE_INCLUDE_DIR SQUIRREL_ENGINE_LIBRARY)
