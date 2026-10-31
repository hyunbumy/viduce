#pragma once

#include <memory>
#include <mutex>

#include "engine/engine_session.h"
#include "engine/v1/engine_service.grpc.pb.h"
#include "engine/v1/engine_service.pb.h"
#include "grpcpp/grpcpp.h"

namespace viduce::engine {

class EngineServiceImpl final : public EngineService::Service {
 public:
  grpc::Status CreateSession(grpc::ServerContext* context,
                             const CreateSessionRequest* request,
                             CreateSessionResponse* response) override;

 private:
  std::unique_ptr<EngineSession> engine_session_;
  std::mutex mutex_;
};

}  // namespace viduce::engine
