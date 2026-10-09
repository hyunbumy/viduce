#include "engine/engine_service_impl.h"

#include "engine/v1/engine_service.pb.h"
#include "grpcpp/grpcpp.h"

namespace viduce::engine {

grpc::Status EngineServiceImpl::CreateSession(
    grpc::ServerContext* context, const CreateSessionRequest* request,
    CreateSessionResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "CreateSession is not implemented");
}

}  // namespace viduce::engine
