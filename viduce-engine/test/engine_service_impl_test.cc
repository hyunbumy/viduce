#include "engine/engine_service_impl.h"

#include <memory>

#include "engine/v1/engine_service.grpc.pb.h"
#include "engine/v1/engine_service.pb.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"

namespace {

using ::testing::HasSubstr;
using ::viduce::engine::CreateSessionRequest;
using ::viduce::engine::CreateSessionResponse;
using ::viduce::engine::EngineService;
using ::viduce::engine::EngineServiceImpl;

TEST(EngineServiceImplTest, CreateSessionReturnsUnimplemented) {
  EngineServiceImpl service;
  grpc::ServerBuilder builder;
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);
  std::unique_ptr<EngineService::Stub> stub = EngineService::NewStub(
      server->InProcessChannel(grpc::ChannelArguments()));

  grpc::ClientContext context;
  CreateSessionResponse response;
  grpc::Status status =
      stub->CreateSession(&context, CreateSessionRequest(), &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
  EXPECT_THAT(status.error_message(), HasSubstr("CreateSession"));

  server->Shutdown();
}

}  // namespace
