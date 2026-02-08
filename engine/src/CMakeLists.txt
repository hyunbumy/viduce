# TODO: Figure out how to encapsulate compilation units
add_library(util STATIC util.cc)
target_include_directories(util PUBLIC ../include)
target_link_libraries(util PRIVATE avutil)

add_library(frame_reader STATIC frame_reader.cc)
target_include_directories(frame_reader PUBLIC ../include)
target_link_libraries(frame_reader PRIVATE absl::status absl::statusor absl::strings avcodec avformat avutil)
target_link_libraries(frame_reader PRIVATE util)

add_library(engine_api SHARED engine_api.cc)
target_include_directories(engine_api PUBLIC ../include)
target_link_libraries(engine_api PRIVATE spdlog::spdlog absl::status absl::statusor avcodec)
target_link_libraries(engine_api PRIVATE frame_reader)

set_target_properties(engine_api PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
