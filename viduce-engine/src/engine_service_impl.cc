#include "engine/engine_service_impl.h"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "engine/v1/engine_service.pb.h"
#include "grpcpp/grpcpp.h"

namespace viduce::engine {

namespace {

// absl and gRPC share the canonical status code numbering.
grpc::Status ToGrpcStatus(const absl::Status& status) {
  return grpc::Status(static_cast<grpc::StatusCode>(status.code()),
                      std::string(status.message()));
}

}  // namespace

grpc::Status EngineServiceImpl::CreateSession(
    grpc::ServerContext* context, const CreateSessionRequest* request,
    CreateSessionResponse* response) {
  if (request->socket_path().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "socket_path is required");
  }

  std::lock_guard<std::mutex> lock(mutex_);
  // Destroy the previous session before starting the new one: destruction
  // joins its socket thread and removes its socket file, which would otherwise
  // remove the new session's file if both use the same path.
  engine_session_.reset();

  auto session = std::make_unique<EngineSession>(request->socket_path());
  if (absl::Status status = session->Start(); !status.ok()) {
    return ToGrpcStatus(status);
  }
  engine_session_ = std::move(session);
  return grpc::Status::OK;
}

}  // namespace viduce::engine
