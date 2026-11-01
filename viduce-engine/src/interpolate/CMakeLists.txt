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

add_library(shm_frame_packer STATIC
    shm_frame_packer.cc
    shm_frame_unpacker.cc
)
target_include_directories(shm_frame_packer PUBLIC ../../include)
target_link_libraries(shm_frame_packer
    PUBLIC
    absl::status
    absl::statusor
    absl::strings
    frame
    shm_buffer
    swscale
    PRIVATE
    avutil
    util
)
