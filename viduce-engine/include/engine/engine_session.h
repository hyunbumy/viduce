#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "boost/asio.hpp"

namespace viduce::engine {

// TODO: Make FrameHeader the canonical ingest -> engine wire format once ingest
// writes to the socket:
// - Rewrite it as a C-compatible header (<stdint.h> types, naturally aligned
//   instead of #pragma pack) with header_size/version/type fields so it can
//   evolve by appending fields.
// - Move it to a shared top-level wire/ directory (e.g. wire/ingest/), next to
//   proto/ rather than inside it.
// - Include it directly in the engine and generate the Rust definition with
//   bindgen in viduce-ingest's build.rs.
// - Document the layout in a spec and test both sides against shared
//   golden-byte fixtures.
#pragma pack(push, 1)
// Header for each frame payload in UDS
struct FrameHeader {
  // Length of the payload
  uint32_t payload_len;
  // Flags for the frame.
  // bit0 = isKeyframe;
  uint32_t flags;
  // Timestamp of the frame in the session's timebase
  int64_t timestamp;
};
#pragma pack(pop)

// EngineSession is responsible for the lifecycle of UDS for ingest and
// triggering the engine to start processing.
//
// The client is expected to close its connection before a started session is
// destroyed; destruction waits for the socket thread to finish.
class EngineSession {
 public:
  // Invoked on the socket thread for each complete frame received.
  using FrameCallback = std::function<void(const FrameHeader& header,
                                           const std::vector<uint8_t>& payload)>;

  explicit EngineSession(std::string_view socket_path,
                         FrameCallback on_frame = nullptr);
  // Joins the socket thread, which ends once the client closes its connection,
  // then removes the socket file if Start() created one.
  ~EngineSession();

  // Starts the session.
  // Binds the socket at `socket_path`, then runs the socket loop in a separate
  // thread and returns immediately. Clients may connect once this returns.
  // Returns InvalidArgument if `socket_path` is empty or too long for a Unix
  // socket, or the errno-mapped error if the socket cannot be bound.
  absl::Status Start();

 private:
  boost::system::error_code ReadFrame(
      boost::asio::local::stream_protocol::socket& socket);

  std::string socket_path_;
  boost::asio::io_context ctx_;
  boost::asio::local::stream_protocol::acceptor acceptor_{ctx_};

  std::unique_ptr<std::thread> socket_thread_;
  std::array<uint8_t, sizeof(FrameHeader)> header_buf_;

  FrameCallback on_frame_;
};

}  // namespace viduce::engine
