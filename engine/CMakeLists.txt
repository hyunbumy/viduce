cmake_minimum_required(VERSION 3.25)

project(viduce_engine)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(deps)

add_subdirectory(src)
if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
