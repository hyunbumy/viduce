#include "engine/engine_session.h"

#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "boost/asio.hpp"
#include "spdlog/spdlog.h"

namespace viduce::engine {

EngineSession::EngineSession(std::string_view socket_path,
                             FrameCallback on_frame)
    : socket_path_(std::string(socket_path)), on_frame_(std::move(on_frame)) {}

EngineSession::~EngineSession() {
  // The socket thread only exists once Start() has bound the socket.
  if (!socket_thread_) {
    return;
  }
  socket_thread_->join();
  // Remove the socket file Start() created.
  ::unlink(socket_path_.c_str());
}

absl::Status EngineSession::Start() {
  if (socket_path_.empty()) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        "Socket path must not be empty");
  }
  // sun_path must also hold the terminating null byte.
  constexpr size_t kMaxSocketPathLen = sizeof(sockaddr_un::sun_path) - 1;
  if (socket_path_.size() > kMaxSocketPathLen) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Socket path %s exceeds the maximum length of %d bytes",
                        socket_path_, kMaxSocketPathLen));
  }

  // Bind before spawning the thread so clients can connect once Start returns.
  ::unlink(socket_path_.c_str());  // Remove previous binding.
  boost::asio::local::stream_protocol::endpoint ep(socket_path_);
  boost::system::error_code ec;
  acceptor_.open(ep.protocol(), ec);
  if (!ec) {
    acceptor_.bind(ep, ec);
  }
  if (!ec) {
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  }
  if (ec) {
    return absl::ErrnoToStatus(
        ec.value(),
        absl::StrFormat("Failed to listen on socket %s", socket_path_));
  }

  socket_thread_ = std::make_unique<std::thread>([this]() {
    boost::asio::local::stream_protocol::socket socket(ctx_);
    acceptor_.accept(socket);

    // Process socket data
    boost::system::error_code ec;
    while (!ec) {
      ec = ReadFrame(socket);
    }

    // Handle eof or error.
    spdlog::info("Socket read loop ended with error code {}", ec.message());

    return;
  });

  return absl::OkStatus();
}

boost::system::error_code EngineSession::ReadFrame(
    boost::asio::local::stream_protocol::socket& socket) {
  boost::system::error_code ec;
  // Read FrameHeader
  boost::asio::read(socket, boost::asio::buffer(header_buf_), ec);
  if (ec) {
    return ec;
  }

  FrameHeader header;
  std::memcpy(&header, header_buf_.data(), sizeof(FrameHeader));

  // Read the payload based on the header
  std::vector<uint8_t> payload(header.payload_len);
  size_t bytes_read =
      boost::asio::read(socket, boost::asio::buffer(payload), ec);
  if (ec) {
    return ec;
  }

  spdlog::info("Received payload of size {}, bytes read {}", payload.size(),
               bytes_read);
  if (on_frame_) {
    on_frame_(header, payload);
  }

  return {};
}

}  // namespace viduce::engine
