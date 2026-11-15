#include "engine/engine_session.h"

#include <sys/un.h>
#include <unistd.h>

#include <memory>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "boost/asio.hpp"
#include "spdlog/spdlog.h"

namespace viduce::engine {

EngineSession::EngineSession(std::string_view socket_path,
                             FrameCallback on_frame)
    : socket_path_(std::string(socket_path)), on_frame_(std::move(on_frame)) {}

absl::StatusOr<std::unique_ptr<EngineSession>> EngineSession::Create(
    std::string_view socket_path, FrameCallback on_frame) {
  // The constructor is private, so std::make_unique cannot be used.
  std::unique_ptr<EngineSession> session(
      new EngineSession(socket_path, std::move(on_frame)));
  if (absl::Status status = session->Listen(); !status.ok()) {
    return status;
  }

  // Queue the first step before starting the thread, so run() has work and
  // does not return immediately.
  session->DoAccept();
  EngineSession* raw = session.get();
  session->io_thread_ = std::thread([raw]() { raw->ctx_.run(); });

  return session;
}

EngineSession::~EngineSession() {
  // The IO thread does not exist if Create() failed to listen.
  if (!io_thread_.joinable()) {
    return;
  }

  // Close on the IO thread so it never races with a running handler. Pending
  // operations then complete with operation_aborted, ending the chain. If the
  // chain already ended, the IO thread has exited and this is never run.
  boost::asio::post(ctx_, [this]() {
    boost::system::error_code ignored;
    acceptor_.close(ignored);
    socket_.close(ignored);
  });
  io_thread_.join();
  // Remove the socket file Listen() created.
  ::unlink(socket_path_.c_str());
}

absl::Status EngineSession::Listen() {
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

  // Bind before Create() spawns the thread so clients can connect once it
  // returns.
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
  return absl::OkStatus();
}

void EngineSession::DoAccept() {
  acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
    if (ec) {
      Finish(ec);
      return;
    }
    DoReadHeader();
  });
}

void EngineSession::DoReadHeader() {
  boost::asio::async_read(
      socket_, boost::asio::buffer(&header_, sizeof(header_)),
      [this](boost::system::error_code ec, size_t /* bytes_read */) {
        if (ec) {
          Finish(ec);
          return;
        }
        payload_.resize(header_.payload_len);
        DoReadPayload();
      });
}

void EngineSession::DoReadPayload() {
  boost::asio::async_read(
      socket_, boost::asio::buffer(payload_),
      [this](boost::system::error_code ec, size_t bytes_read) {
        if (ec) {
          Finish(ec);
          return;
        }
        spdlog::info("Received payload of size {}, bytes read {}",
                     payload_.size(), bytes_read);
        if (on_frame_) {
          on_frame_(header_, payload_);
        }
        DoReadHeader();
      });
}

void EngineSession::Finish(const boost::system::error_code& ec) {
  // Handle eof, abort by the destructor, or error.
  spdlog::info("Socket read loop ended with error code {}", ec.message());
  boost::system::error_code ignored;
  socket_.close(ignored);
}

}  // namespace viduce::engine
