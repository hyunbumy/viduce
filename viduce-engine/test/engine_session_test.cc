#include "engine/engine_session.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "boost/asio.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::viduce::engine::EngineSession;
using ::viduce::engine::FrameHeader;

struct ReceivedFrame {
  FrameHeader header;
  std::vector<uint8_t> payload;
};

// Returns a socket path unique to the running test.
std::string TestSocketPath() {
  return ::testing::TempDir() + "engine_session_test_" +
         ::testing::UnitTest::GetInstance()->current_test_info()->name() +
         ".sock";
}

// Appends a frame (header followed by payload) to `bytes`.
void AppendFrame(std::vector<uint8_t>& bytes, uint32_t flags,
                 int64_t timestamp, const std::vector<uint8_t>& payload) {
  FrameHeader header{static_cast<uint32_t>(payload.size()), flags, timestamp};
  const auto* header_bytes = reinterpret_cast<const uint8_t*>(&header);
  bytes.insert(bytes.end(), header_bytes, header_bytes + sizeof(header));
  bytes.insert(bytes.end(), payload.begin(), payload.end());
}

// Ends the client's side of the stream, then waits until the session closes its
// side. The session only does that once it has read everything up to the end
// of the stream, so all sent frames are processed when this returns.
void DisconnectAndWaitForSession(
    boost::asio::local::stream_protocol::socket& client) {
  client.shutdown(boost::asio::socket_base::shutdown_send);
  uint8_t unused;
  boost::system::error_code ec;
  boost::asio::read(client, boost::asio::buffer(&unused, 1), ec);
  EXPECT_EQ(ec, boost::asio::error::eof);
  client.close();
}

// Starts a session, writes `bytes` to it from a client, then disconnects.
// Returns the frames the session received.
std::vector<ReceivedFrame> ExerciseSession(const std::vector<uint8_t>& bytes) {
  std::string socket_path = TestSocketPath();
  std::vector<ReceivedFrame> frames;
  {
    absl::StatusOr<std::unique_ptr<EngineSession>> session =
        EngineSession::Create(socket_path,
                              [&frames](const FrameHeader& header,
                                        const std::vector<uint8_t>& payload) {
                                frames.push_back({header, payload});
                              });
    if (!session.ok()) {
      ADD_FAILURE() << "Create failed: " << session.status();
      return frames;
    }

    boost::asio::io_context ctx;
    boost::asio::local::stream_protocol::socket client(ctx);
    client.connect(boost::asio::local::stream_protocol::endpoint(socket_path));
    boost::asio::write(client, boost::asio::buffer(bytes));
    DisconnectAndWaitForSession(client);
  }
  return frames;
}

TEST(FrameHeaderTest, IsPackedTo16Bytes) {
  EXPECT_EQ(sizeof(FrameHeader), 16);
}

TEST(EngineSessionTest, DeliversFrameHeaderAndPayload) {
  std::vector<uint8_t> bytes;
  AppendFrame(bytes, /*flags=*/1, /*timestamp=*/42, {0xde, 0xad, 0xbe, 0xef});

  std::vector<ReceivedFrame> frames = ExerciseSession(bytes);

  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0].header.payload_len, 4);
  EXPECT_EQ(frames[0].header.flags, 1);
  EXPECT_EQ(frames[0].header.timestamp, 42);
  EXPECT_THAT(frames[0].payload, ElementsAre(0xde, 0xad, 0xbe, 0xef));
}

TEST(EngineSessionTest, DeliversMultipleFramesInOrder) {
  std::vector<uint8_t> bytes;
  AppendFrame(bytes, /*flags=*/1, /*timestamp=*/0, {1});
  AppendFrame(bytes, /*flags=*/0, /*timestamp=*/33, {2, 2});
  AppendFrame(bytes, /*flags=*/0, /*timestamp=*/66, {3, 3, 3});

  std::vector<ReceivedFrame> frames = ExerciseSession(bytes);

  ASSERT_EQ(frames.size(), 3);
  EXPECT_EQ(frames[0].header.timestamp, 0);
  EXPECT_THAT(frames[0].payload, ElementsAre(1));
  EXPECT_EQ(frames[1].header.timestamp, 33);
  EXPECT_THAT(frames[1].payload, ElementsAre(2, 2));
  EXPECT_EQ(frames[2].header.timestamp, 66);
  EXPECT_THAT(frames[2].payload, ElementsAre(3, 3, 3));
}

TEST(EngineSessionTest, DeliversFrameWithEmptyPayload) {
  std::vector<uint8_t> bytes;
  AppendFrame(bytes, /*flags=*/0, /*timestamp=*/7, {});

  std::vector<ReceivedFrame> frames = ExerciseSession(bytes);

  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0].header.payload_len, 0);
  EXPECT_EQ(frames[0].header.timestamp, 7);
  EXPECT_THAT(frames[0].payload, IsEmpty());
}

TEST(EngineSessionTest, DropsFrameWithTruncatedPayload) {
  std::vector<uint8_t> bytes;
  AppendFrame(bytes, /*flags=*/0, /*timestamp=*/0,
              {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  bytes.resize(bytes.size() - 7);  // Send only 3 of the 10 payload bytes.

  std::vector<ReceivedFrame> frames = ExerciseSession(bytes);

  EXPECT_THAT(frames, IsEmpty());
}

TEST(EngineSessionTest, DropsTruncatedHeader) {
  std::vector<uint8_t> bytes(sizeof(FrameHeader) - 1, 0);

  std::vector<ReceivedFrame> frames = ExerciseSession(bytes);

  EXPECT_THAT(frames, IsEmpty());
}

TEST(EngineSessionTest, EndsWhenClientDisconnectsWithoutSending) {
  std::vector<ReceivedFrame> frames = ExerciseSession({});

  EXPECT_THAT(frames, IsEmpty());
}

TEST(EngineSessionTest, RemovesSocketFileWhenDestroyed) {
  // Connecting inside ExerciseSession proves the socket file existed.
  ExerciseSession({});

  EXPECT_FALSE(std::filesystem::exists(TestSocketPath()));
}

TEST(EngineSessionTest, DestructionReturnsWhenNoClientConnected) {
  std::string socket_path = TestSocketPath();
  absl::StatusOr<std::unique_ptr<EngineSession>> session =
      EngineSession::Create(socket_path);
  ASSERT_TRUE(session.ok()) << session.status();

  session->reset();

  EXPECT_FALSE(std::filesystem::exists(socket_path));
}

TEST(EngineSessionTest, DestructionReturnsWhenClientIsConnectedButIdle) {
  std::string socket_path = TestSocketPath();
  absl::StatusOr<std::unique_ptr<EngineSession>> session =
      EngineSession::Create(socket_path);
  ASSERT_TRUE(session.ok()) << session.status();
  boost::asio::io_context ctx;
  boost::asio::local::stream_protocol::socket client(ctx);
  client.connect(boost::asio::local::stream_protocol::endpoint(socket_path));

  session->reset();

  EXPECT_FALSE(std::filesystem::exists(socket_path));
}

TEST(EngineSessionTest, CreateRejectsEmptySocketPath) {
  EXPECT_THAT(EngineSession::Create(""),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(EngineSessionTest, CreateRejectsTooLongSocketPath) {
  EXPECT_THAT(
      EngineSession::Create(::testing::TempDir() + std::string(200, 'a')),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(EngineSessionTest, CreateFailsWhenDirectoryDoesNotExist) {
  EXPECT_THAT(
      EngineSession::Create(::testing::TempDir() + "missing_dir/session.sock"),
      StatusIs(absl::StatusCode::kNotFound));
}

}  // namespace
