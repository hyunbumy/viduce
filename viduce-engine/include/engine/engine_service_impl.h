#pragma once

#include "engine/v1/engine_service.grpc.pb.h"
#include "engine/v1/engine_service.pb.h"
#include "grpcpp/grpcpp.h"

namespace viduce::engine {

// Implementation of the EngineService gRPC API. Every RPC currently returns
// UNIMPLEMENTED.
class EngineServiceImpl final : public EngineService::Service {
 public:
  grpc::Status CreateSession(grpc::ServerContext* context,
                             const CreateSessionRequest* request,
                             CreateSessionResponse* response) override;
};

}  // namespace viduce::engine
