cmake_minimum_required(VERSION 3.0)

project(raft_demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
set(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")

if(CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_BUILD_POSTFIX "${CMAKE_RELEASE_POSTFIX}")
elseif(CMAKE_BUILD_TYPE MATCHES "DEBUG")
    set(CMAKE_BUILD_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
else()
    set(CMAKE_BUILD_POSTFIX "")
endif()

if(CMAKE_DEBUG_POSTFIX)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DRW_LIBRARY_POSTFIX=${CMAKE_DEBUG_POSTFIX}")
endif()
if(CMAKE_RELEASE_POSTFIX)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRW_LIBRARY_POSTFIX=${CMAKE_RELEASE_POSTFIX}")
endif()

if(GNU)
    set(MEM_DEBUG_MONITOR "-fsanitize=address")
    set(CMAKE_CXX_COMPILER g++-9)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -L/usr/local/lib -pthread -g -O1 -Wreturn-type -Wl,-rpath=./3rdlib/lib -DNDEUG")
endif(GNU)

set(PRO_LIB_NAME raftlib)

set(DEBUG_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin/debug)
set(RELEASE_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin/release)

set(PRO_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(Lib_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(Lib_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/include)

set(THIRD_LIB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdlib/include)

include_directories(${Lib_SRC_DIR})
include_directories(${Lib_INC_DIR})

add_subdirectory(src)
add_subdirectory(sample)
add_subdirectory(test)
