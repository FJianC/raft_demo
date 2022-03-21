cmake_minimum_required(VERSION 3.0)

project(sample)

set(CMAKE_DEBUG_POSTFIX "_d" CACHE STRING "add a postfix, usually d on windows")
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
    set(CORE_LIB raftlibd)
endif()
if(CMAKE_RELEASE_POSTFIX)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRW_LIBRARY_POSTFIX=${CMAKE_RELEASE_POSTFIX}")
    set(CORE_LIB raftlib)
endif()

set(CORE_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/lib/${CMAKE_BUILD_TYPE}/)
set(CORE_LIB_INC ${CMAKE_CURRENT_SOURCE_DIR}/../src/include/)

set(SAMPLE_LIST
    thread_pool_sample
)

link_directories(${PRO_LIB_DIR})

foreach(src ${SAMPLE_LIST})
    string(REPLACE "-" ";" arr ${src})
    list(GET arr -1 BIN_NAME)
    add_executable(${BIN_NAME} ${src}.cc)
    target_link_libraries(${BIN_NAME} ${CORE_LIB})
    set_target_properties(
        ${BIN_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DEBUG_BIN_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RELEASE_BIN_DIR}
    )
    add_dependencies(${BIN_NAME} ${PRO_LIB_NAME})
    if(MSVC)
        target_compile_definitions(
            ${BIN_NAME} PRIVATE
            strdup=_strdup
            strcasecmp=_stricmp
            strncasecmp=_strnicmp
        )
    else()
        target_link_libraries(${BIN_NAME} pthread)
    endif()
endforeach()
