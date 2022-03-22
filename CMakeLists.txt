project(plankton CXX)
set(TOOL_NAME plankton)
set(INSTALL_FOLDER plankton)

# built requirements
cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set default release type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

# remain in testing mode for the moment
# set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE RelWithDebInfo)
# set(CMAKE_VERBOSE_MAKEFILE ON)

# optimization in release mode
# include(CheckCXXCompilerFlag)
# CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
# if(COMPILER_SUPPORTS_MARCH_NATIVE)
#     set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
# endif()

# be verbose in debug mode
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_DEBUG_PRINTING")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_TIMER")

# configure compiler
add_definitions(-fno-strict-aliasing)
add_definitions(-Wall -Wextra)
add_definitions(-fshow-column)
# add_definitions(-Wno-unused-parameter -Wno-unused-function)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# includes
include_directories(include)

# sources/executables
add_subdirectory(src)

# installation
#install(FILES benchmark.py DESTINATION ${INSTALL_FOLDER})
#install(DIRECTORY examples DESTINATION ${INSTALL_FOLDER})
