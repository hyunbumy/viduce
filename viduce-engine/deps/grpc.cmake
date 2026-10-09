# Protobuf and gRPC discovery for viduce_engine

find_package(Protobuf REQUIRED)
if(TARGET protobuf::libprotobuf)
    set_target_properties(protobuf::libprotobuf PROPERTIES IMPORTED_GLOBAL TRUE)
endif()

# Try CONFIG mode first (e.g. if installed with cmake configs), fall back to PkgConfig
find_package(gRPC CONFIG QUIET)

if(NOT gRPC_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET GLOBAL grpc++)
    
    if(NOT TARGET gRPC::grpc++)
        add_library(gRPC::grpc++ INTERFACE IMPORTED GLOBAL)
        target_link_libraries(gRPC::grpc++ INTERFACE PkgConfig::GRPC)
    endif()
else()
    if(TARGET gRPC::grpc++)
        set_target_properties(gRPC::grpc++ PROPERTIES IMPORTED_GLOBAL TRUE)
    endif()
endif()

# Locate the gRPC C++ protoc plugin
find_program(GRPC_CPP_PLUGIN
    NAMES grpc_cpp_plugin
    DOC "Path to the gRPC C++ protoc plugin"
    REQUIRED
)

message(STATUS "Protobuf compiler: ${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "gRPC C++ plugin: ${GRPC_CPP_PLUGIN}")
