#include "engine/engine_service_impl.h"

#include <string>

#include "boost/asio.hpp"
#include "engine/v1/engine_service.pb.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"

namespace {

using ::testing::HasSubstr;
using ::viduce::engine::CreateSessionRequest;
using ::viduce::engine::CreateSessionResponse;
using ::viduce::engine::EngineServiceImpl;

// Returns a socket path unique to the running test and `suffix`.
std::string TestSocketPath(const std::string& suffix = "") {
  return ::testing::TempDir() + "engine_service_impl_test_" +
         ::testing::UnitTest::GetInstance()->current_test_info()->name() +
         suffix + ".sock";
}

// Connects to `socket_path` and disconnects, which ends the session's read
// loop so the session can be destroyed.
boost::system::error_code ConnectAndDisconnect(const std::string& socket_path) {
  boost::asio::io_context ctx;
  boost::asio::local::stream_protocol::socket client(ctx);
  boost::system::error_code ec;
  client.connect(boost::asio::local::stream_protocol::endpoint(socket_path),
                 ec);
  return ec;
}

// Calls the CreateSession handler directly, without a gRPC server.
grpc::Status CreateSession(EngineServiceImpl& service,
                           const CreateSessionRequest& request) {
  grpc::ServerContext context;
  CreateSessionResponse response;
  return service.CreateSession(&context, &request, &response);
}

TEST(EngineServiceImplTest, CreateSessionRejectsMissingSocketPath) {
  EngineServiceImpl service;

  grpc::Status status = CreateSession(service, CreateSessionRequest());

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(status.error_message(), HasSubstr("socket_path"));
}

TEST(EngineServiceImplTest, CreateSessionRejectsEmptySocketPath) {
  EngineServiceImpl service;
  CreateSessionRequest request;
  request.set_socket_path("");

  grpc::Status status = CreateSession(service, request);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(status.error_message(), HasSubstr("socket_path"));
}

TEST(EngineServiceImplTest, CreateSessionListensOnSocketPath) {
  EngineServiceImpl service;
  CreateSessionRequest request;
  request.set_socket_path(TestSocketPath());

  grpc::Status status = CreateSession(service, request);

  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_FALSE(ConnectAndDisconnect(request.socket_path()));
}

TEST(EngineServiceImplTest, CreateSessionReplacesExistingSession) {
  EngineServiceImpl service;
  CreateSessionRequest first;
  first.set_socket_path(TestSocketPath("_first"));
  ASSERT_TRUE(CreateSession(service, first).ok());
  ASSERT_FALSE(ConnectAndDisconnect(first.socket_path()));

  CreateSessionRequest second;
  second.set_socket_path(TestSocketPath("_second"));
  grpc::Status status = CreateSession(service, second);

  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_FALSE(ConnectAndDisconnect(second.socket_path()));
}

TEST(EngineServiceImplTest, CreateSessionReusesSocketPathOfExistingSession) {
  EngineServiceImpl service;
  CreateSessionRequest request;
  request.set_socket_path(TestSocketPath());
  ASSERT_TRUE(CreateSession(service, request).ok());
  ASSERT_FALSE(ConnectAndDisconnect(request.socket_path()));

  grpc::Status status = CreateSession(service, request);

  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_FALSE(ConnectAndDisconnect(request.socket_path()));
}

TEST(EngineServiceImplTest, CreateSessionReturnsStartError) {
  EngineServiceImpl service;
  CreateSessionRequest request;
  request.set_socket_path(::testing::TempDir() + "missing_dir/session.sock");

  grpc::Status status = CreateSession(service, request);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
  EXPECT_THAT(status.error_message(), HasSubstr(request.socket_path()));
}

}  // namespace
