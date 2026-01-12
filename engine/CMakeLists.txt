cmake_minimum_required(VERSION 3.25)

project(viduce_engine)

add_subdirectory(deps)

add_library(engine_api SHARED engine_api.cc)
target_include_directories(engine_api PRIVATE ./)
target_link_libraries(engine_api PRIVATE spdlog::spdlog avcodec avformat)

add_executable(exec main.cc)
target_link_libraries(exec PRIVATE engine_api)
