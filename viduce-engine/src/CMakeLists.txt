# TODO: Figure out how to encapsulate compilation units

add_subdirectory(interpolate)
add_subdirectory(upscale)

# C++ protobuf/gRPC code for EngineService, generated into the build tree from
# the repo-root proto/ tree. Protobuf/gRPC discovery and GRPC_CPP_PLUGIN come
# from deps/grpc.cmake. protoc is invoked directly because CMake 3.25's
# protobuf_generate assumes protos live under the current source directory.
set(ENGINE_SERVICE_PROTO_ROOT "${CMAKE_SOURCE_DIR}/../proto")
set(ENGINE_SERVICE_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto_gen")
set(ENGINE_SERVICE_PROTO "${ENGINE_SERVICE_PROTO_ROOT}/engine/v1/engine_service.proto")
set(ENGINE_SERVICE_GEN "${ENGINE_SERVICE_GEN_DIR}/engine/v1/engine_service")
file(MAKE_DIRECTORY "${ENGINE_SERVICE_GEN_DIR}")
add_custom_command(
    OUTPUT
        "${ENGINE_SERVICE_GEN}.pb.h"
        "${ENGINE_SERVICE_GEN}.pb.cc"
        "${ENGINE_SERVICE_GEN}.grpc.pb.h"
        "${ENGINE_SERVICE_GEN}.grpc.pb.cc"
    COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
        -I "${ENGINE_SERVICE_PROTO_ROOT}"
        --cpp_out "${ENGINE_SERVICE_GEN_DIR}"
        --grpc_out "${ENGINE_SERVICE_GEN_DIR}"
        "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
        "${ENGINE_SERVICE_PROTO}"
    DEPENDS "${ENGINE_SERVICE_PROTO}"
    VERBATIM
)
add_library(engine_service_proto STATIC
    "${ENGINE_SERVICE_GEN}.pb.cc"
    "${ENGINE_SERVICE_GEN}.grpc.pb.cc"
)
target_include_directories(engine_service_proto PUBLIC "${ENGINE_SERVICE_GEN_DIR}")
target_link_libraries(engine_service_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)

add_library(engine_service_impl STATIC engine_service_impl.cc)
target_include_directories(engine_service_impl PUBLIC ../include)
target_link_libraries(engine_service_impl PUBLIC engine_service_proto engine_session)
target_link_libraries(engine_service_impl PRIVATE absl::status)

add_library(engine_session STATIC engine_session.cpp)
target_include_directories(engine_session PUBLIC ../include)
target_link_libraries(engine_session PUBLIC absl::status Boost::asio)
target_link_libraries(engine_session PRIVATE absl::str_format spdlog::spdlog)

add_executable(engine_server engine_server.cc)
target_link_libraries(engine_server
    PRIVATE
    absl::flags
    absl::flags_parse
    engine_service_impl
    spdlog::spdlog
)

add_library(util STATIC util.cc)
target_include_directories(util PUBLIC ../include)
target_link_libraries(util PRIVATE avutil)

add_library(frame STATIC frame.cc)
target_include_directories(frame PUBLIC ../include)
target_link_libraries(frame
    PRIVATE
    absl::statusor
    avcodec
    avformat
    avutil
)

add_library(frame_reader STATIC frame_reader.cc)
target_include_directories(frame_reader PUBLIC ../include)
target_link_libraries(frame_reader
    PRIVATE
    absl::status
    absl::statusor
    absl::strings
    avcodec
    avformat
    avutil
    swscale
)
target_link_libraries(frame_reader PRIVATE frame util)

add_library(frame_writer STATIC frame_writer.cc)
target_include_directories(frame_writer PUBLIC ../include)
target_link_libraries(frame_writer
    PRIVATE
    absl::status
    absl::statusor
    absl::strings
    avcodec
    avformat
    avutil
    spdlog::spdlog
    swscale
)
target_link_libraries(frame_writer PRIVATE frame util)

# Protobuf & gRPC code generation for LDF-VFI service
set(PROTO_FILE "${CMAKE_SOURCE_DIR}/../proto/ldfvfi/v1/service.proto")
set(PROTO_DIR "${CMAKE_SOURCE_DIR}/../proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR}/ldfvfi/v1)

set(PROTO_SRCS "${PROTO_GEN_DIR}/ldfvfi/v1/service.pb.cc")
set(PROTO_HDRS "${PROTO_GEN_DIR}/ldfvfi/v1/service.pb.h")
set(GRPC_SRCS "${PROTO_GEN_DIR}/ldfvfi/v1/service.grpc.pb.cc")
set(GRPC_HDRS "${PROTO_GEN_DIR}/ldfvfi/v1/service.grpc.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --proto_path=${PROTO_DIR}
        --cpp_out=${PROTO_GEN_DIR}
        --grpc_out=${PROTO_GEN_DIR}
        --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating C++ Protobuf and gRPC stubs from service.proto"
    VERBATIM
)

add_library(proto_grpc STATIC
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${GRPC_SRCS}
    ${GRPC_HDRS}
)
target_include_directories(proto_grpc SYSTEM PUBLIC
    $<BUILD_INTERFACE:${PROTO_GEN_DIR}>
    $<BUILD_INTERFACE:${PROTO_GEN_DIR}/ldfvfi/v1>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
target_link_libraries(proto_grpc PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)

add_library(engine_api SHARED engine_api.cc)
target_include_directories(engine_api PUBLIC ../include)
target_link_libraries(engine_api
    PRIVATE
    absl::status
    absl::statusor
    avcodec
    avutil
    spdlog::spdlog
)
target_link_libraries(engine_api
    PRIVATE
    frame_reader
    frame_writer
    ldf_vfi_client
    model_impl
    proto_grpc
    shm_buffer
    shm_frame_packer
    upscaler
)

set_target_properties(engine_api PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_executable(demo demo.cc)
target_link_libraries(demo PRIVATE engine_api)
target_compile_options(demo PRIVATE -fsanitize=address)
target_link_options(demo PRIVATE -fsanitize=address)
