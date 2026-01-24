add_library(engine_api SHARED engine_api.cc)
target_include_directories(engine_api PUBLIC ../include)
target_link_libraries(engine_api PRIVATE spdlog::spdlog absl::status absl::strings avcodec avformat avutil)
