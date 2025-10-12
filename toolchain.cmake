# --- Specify compiler (change these if you prefer Clang)
set(CMAKE_C_COMPILER gcc CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER g++ CACHE STRING "C++ compiler")

# --- Default build type if not set
if(NOT CMAKE_BUILD_TYPE)
 set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- General compile options (apply to all configurations)
set(COMMON_C_FLAGS "-Wall -Wextra -Wpedantic -fPIC")
set(COMMON_CXX_FLAGS "-Wall -Wextra -Wpedantic -fPIC -std=c++20")

# --- Flags per configuration
set(CMAKE_C_FLAGS_DEBUG "${COMMON_C_FLAGS} -O0 -g3" CACHE STRING "Debug C flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "${COMMON_C_FLAGS} -O3 -DNDEBUG" CACHE STRING "Release C flags" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${COMMON_C_FLAGS} -O2 -g -DNDEBUG" CACHE STRING "RelWithDebInfo C flags" FORCE)
set(CMAKE_C_FLAGS_MINSIZEREL "${COMMON_C_FLAGS} -Os -DNDEBUG" CACHE STRING "MinSizeRel C flags" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "${COMMON_CXX_FLAGS} -O0 -g3" CACHE STRING "Debug C++ flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_CXX_FLAGS} -O3 -DNDEBUG" CACHE STRING "Release C++ flags" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${COMMON_CXX_FLAGS} -O2 -g -DNDEBUG" CACHE STRING "RelWithDebInfo C++ flags" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "${COMMON_CXX_FLAGS} -Os -DNDEBUG" CACHE STRING "MinSizeRel C++ flags" FORCE)

# --- Output directories (relative to the build directory)
# These control where compiled binaries/libraries are placed.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Executable output dir" FORCE)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE PATH "Shared library output dir" FORCE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE PATH "Static library output dir" FORCE)