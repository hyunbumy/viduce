#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace viduce::engine::interpolate {

// RAII wrapper around a POSIX shared memory mapped segment.
// Represents a raw contiguous memory buffer mapped into the process virtual
// address space at byte offset 0.
class ShmBuffer {
 public:
  // Attaches to an existing shared memory segment created by the server.
  // Validates that the segment size is at least `min_size` bytes.
  static absl::StatusOr<std::unique_ptr<ShmBuffer>> Open(
      std::string_view name, size_t min_size, bool read_only = false);

  // Creates and truncates a new shared memory segment.
  // Intended for tests and local segment allocation.
  static absl::StatusOr<std::unique_ptr<ShmBuffer>> Create(
      std::string_view name, size_t size);

  // Unlinks a shared memory segment by name from the OS /dev/shm.
  static absl::Status Unlink(std::string_view name);

  ~ShmBuffer();

  // Move-only semantics.
  ShmBuffer(ShmBuffer&& other) noexcept;
  ShmBuffer& operator=(ShmBuffer&& other) noexcept;

  ShmBuffer(const ShmBuffer&) = delete;
  ShmBuffer& operator=(const ShmBuffer&) = delete;

  // Direct pointer to the mapped contiguous buffer.
  uint8_t* data() { return data_; }
  const uint8_t* data() const { return data_; }

  // Total mapped size in bytes.
  size_t size() const { return size_; }

  // POSIX shared memory name (e.g. "/viduce_abc123_in").
  std::string_view name() const { return name_; }

  // Whether the buffer was opened in read-only mode.
  bool is_read_only() const { return read_only_; }

  // Writes `frame_bytes` of contiguous pixel data into the buffer at
  // `frame_index * frame_bytes`.
  absl::Status WriteFrame(size_t frame_index, const uint8_t* src,
                          size_t frame_bytes);

  // Reads `frame_bytes` of contiguous pixel data from the buffer at
  // `frame_index * frame_bytes` into `dest`.
  absl::Status ReadFrame(size_t frame_index, uint8_t* dest,
                         size_t frame_bytes) const;

 private:
  ShmBuffer(std::string name, int fd, uint8_t* data, size_t size,
            bool read_only, bool owns_unlink);

  void Release();

  std::string name_;
  int fd_ = -1;
  uint8_t* data_ = nullptr;
  size_t size_ = 0;
  bool read_only_ = false;
  bool owns_unlink_ = false;
};

}  // namespace viduce::engine::interpolate
