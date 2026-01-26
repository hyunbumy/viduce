cmake_minimum_required(VERSION 3.25)

project(viduce_engine)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_TESTING "" OFF)
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    set(BUILD_TESTING ON)
    enable_testing()
endif()

add_subdirectory(deps)

add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(test)
endif()
