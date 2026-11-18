add_library(shm_buffer STATIC shm_buffer.cc)
target_include_directories(shm_buffer PUBLIC ../../include)
target_link_libraries(shm_buffer
    PUBLIC
    absl::status
    absl::statusor
    absl::strings
    PRIVATE
    rt
)
