add_library(upscaler STATIC upscaler.cc)
target_include_directories(upscaler PUBLIC ../../include)
target_link_libraries(upscaler
    PRIVATE
    absl::status
    absl::statusor
)
target_link_libraries(upscaler PRIVATE frame util)

add_library(model_impl STATIC model_impl.cc)
target_include_directories(model_impl PUBLIC ../../include)
target_link_libraries(model_impl
    PRIVATE
    absl::status
    absl::statusor
    avutil
    litert
)
