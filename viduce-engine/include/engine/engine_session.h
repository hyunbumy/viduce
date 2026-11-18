#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
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
// A session is running for as long as it exists: Create() binds the socket and
// starts accepting, and destruction stops it. Socket I/O runs asynchronously on
// a dedicated IO thread, so destruction returns promptly whether or not a
// client has connected, and whether or not the client is still sending.
class EngineSession {
 public:
  // Invoked on the IO thread for each complete frame received.
  using FrameCallback = std::function<void(const FrameHeader& header,
                                           const std::vector<uint8_t>& payload)>;

  // Binds the socket at `socket_path`, then accepts and reads from a client on
  // the IO thread. Clients may connect once this returns.
  // Returns InvalidArgument if `socket_path` is empty or too long for a Unix
  // socket, or the errno-mapped error if the socket cannot be bound.
  static absl::StatusOr<std::unique_ptr<EngineSession>> Create(
      std::string_view socket_path, FrameCallback on_frame = nullptr);

  // Aborts any pending accept or read, joins the IO thread, then removes the
  // socket file. Unread frames are dropped. Must not run on the IO thread, i.e.
  // the session must not be destroyed from the frame callback.
  ~EngineSession();

  EngineSession(const EngineSession&) = delete;
  EngineSession& operator=(const EngineSession&) = delete;

 private:
  EngineSession(std::string_view socket_path, FrameCallback on_frame);

  // Validates the socket path, then binds and listens on it.
  absl::Status Listen();

  void DoAccept();
  void DoReadHeader();
  void DoReadPayload();
  void Finish(const boost::system::error_code& ec);

  std::string socket_path_;
  FrameCallback on_frame_;

  // Must be declared before the acceptor and socket, which are bound to it and
  // must be destroyed before it.
  boost::asio::io_context ctx_;
  boost::asio::local::stream_protocol::acceptor acceptor_{ctx_};
  boost::asio::local::stream_protocol::socket socket_{ctx_};
  std::thread io_thread_;

  // Pre-allocated FrameHeader instance reused for reading from the socket
  FrameHeader header_;
  // Pre-allocated payload buffer for the read frame
  std::vector<uint8_t> payload_;
};

}  // namespace viduce::engine
