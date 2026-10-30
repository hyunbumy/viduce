#include "engine/interpolate/shm_buffer.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace viduce::engine::interpolate {

namespace {

std::string NormalizeShmName(std::string_view name) {
  if (name.empty()) {
    return "/viduce_unnamed_shm";
  }
  if (name.front() == '/') {
    return std::string(name);
  }
  return absl::StrCat("/", name);
}

absl::Status ErrnoToStatus(int err, std::string_view action,
                           std::string_view name) {
  std::string msg = absl::StrFormat("Failed to %s '%s': %s (errno %d)", action,
                                    name, std::strerror(err), err);
  switch (err) {
    case ENOENT:
      return absl::NotFoundError(msg);
    case EACCES:
    case EPERM:
      return absl::PermissionDeniedError(msg);
    case EEXIST:
      return absl::AlreadyExistsError(msg);
    case EINVAL:
      return absl::InvalidArgumentError(msg);
    case ENOMEM:
      return absl::ResourceExhaustedError(msg);
    default:
      return absl::InternalError(msg);
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<ShmBuffer>> ShmBuffer::Open(
    std::string_view name, size_t min_size, bool read_only) {
  std::string norm_name = NormalizeShmName(name);
  int flags = read_only ? O_RDONLY : O_RDWR;
  int fd = ::shm_open(norm_name.c_str(), flags, 0);
  if (fd < 0) {
    return ErrnoToStatus(errno, "open shared memory", norm_name);
  }

  struct stat sb;
  if (::fstat(fd, &sb) < 0) {
    int err = errno;
    ::close(fd);
    return ErrnoToStatus(err, "stat shared memory", norm_name);
  }

  size_t actual_size = static_cast<size_t>(sb.st_size);
  if (min_size > 0 && actual_size < min_size) {
    ::close(fd);
    return absl::FailedPreconditionError(absl::StrFormat(
        "Shared memory segment '%s' size (%zu bytes) is smaller than requested "
        "min_size (%zu bytes)",
        norm_name, actual_size, min_size));
  }

  size_t map_size = (min_size > 0) ? min_size : actual_size;
  if (map_size == 0) {
    ::close(fd);
    return absl::InvalidArgumentError(absl::StrFormat(
        "Shared memory segment '%s' has zero size; cannot map", norm_name));
  }

  int prot = PROT_READ;
  if (!read_only) {
    prot |= PROT_WRITE;
  }

  void* addr = ::mmap(nullptr, map_size, prot, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    int err = errno;
    ::close(fd);
    return ErrnoToStatus(err, "mmap shared memory", norm_name);
  }

  return std::unique_ptr<ShmBuffer>(new ShmBuffer(
      std::move(norm_name), fd, static_cast<uint8_t*>(addr), map_size,
      read_only, /*owns_unlink=*/false));
}

absl::StatusOr<std::unique_ptr<ShmBuffer>> ShmBuffer::Create(
    std::string_view name, size_t size) {
  if (size == 0) {
    return absl::InvalidArgumentError("Shared memory size must be positive");
  }

  std::string norm_name = NormalizeShmName(name);
  // Unlink any existing stale segment with the same name.
  ::shm_unlink(norm_name.c_str());

  int fd = ::shm_open(norm_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660);
  if (fd < 0) {
    return ErrnoToStatus(errno, "create shared memory", norm_name);
  }

  if (::ftruncate(fd, static_cast<off_t>(size)) < 0) {
    int err = errno;
    ::close(fd);
    ::shm_unlink(norm_name.c_str());
    return ErrnoToStatus(err, "truncate shared memory", norm_name);
  }

  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    int err = errno;
    ::close(fd);
    ::shm_unlink(norm_name.c_str());
    return ErrnoToStatus(err, "mmap shared memory", norm_name);
  }

  return std::unique_ptr<ShmBuffer>(new ShmBuffer(
      std::move(norm_name), fd, static_cast<uint8_t*>(addr), size,
      /*read_only=*/false, /*owns_unlink=*/true));
}

absl::Status ShmBuffer::Unlink(std::string_view name) {
  std::string norm_name = NormalizeShmName(name);
  if (::shm_unlink(norm_name.c_str()) < 0) {
    int err = errno;
    if (err == ENOENT) {
      return absl::OkStatus();
    }
    return ErrnoToStatus(err, "unlink shared memory", norm_name);
  }
  return absl::OkStatus();
}

ShmBuffer::ShmBuffer(std::string name, int fd, uint8_t* data, size_t size,
                     bool read_only, bool owns_unlink)
    : name_(std::move(name)),
      fd_(fd),
      data_(data),
      size_(size),
      read_only_(read_only),
      owns_unlink_(owns_unlink) {}

ShmBuffer::~ShmBuffer() {
  Release();
}

ShmBuffer::ShmBuffer(ShmBuffer&& other) noexcept
    : name_(std::move(other.name_)),
      fd_(other.fd_),
      data_(other.data_),
      size_(other.size_),
      read_only_(other.read_only_),
      owns_unlink_(other.owns_unlink_) {
  other.fd_ = -1;
  other.data_ = nullptr;
  other.size_ = 0;
  other.owns_unlink_ = false;
}

ShmBuffer& ShmBuffer::operator=(ShmBuffer&& other) noexcept {
  if (this != &other) {
    Release();
    name_ = std::move(other.name_);
    fd_ = other.fd_;
    data_ = other.data_;
    size_ = other.size_;
    read_only_ = other.read_only_;
    owns_unlink_ = other.owns_unlink_;

    other.fd_ = -1;
    other.data_ = nullptr;
    other.size_ = 0;
    other.owns_unlink_ = false;
  }
  return *this;
}

void ShmBuffer::Release() {
  if (data_ != nullptr && data_ != MAP_FAILED) {
    ::munmap(data_, size_);
    data_ = nullptr;
  }
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  if (owns_unlink_ && !name_.empty()) {
    ::shm_unlink(name_.c_str());
    owns_unlink_ = false;
  }
}

absl::Status ShmBuffer::WriteFrame(size_t frame_index, const uint8_t* src,
                                   size_t frame_bytes) {
  if (read_only_) {
    return absl::PermissionDeniedError(
        absl::StrFormat("Cannot write to read-only ShmBuffer '%s'", name_));
  }
  if (src == nullptr) {
    return absl::InvalidArgumentError("Source frame pointer is null");
  }
  if (frame_bytes == 0) {
    return absl::InvalidArgumentError("frame_bytes must be positive");
  }

  size_t offset = frame_index * frame_bytes;
  if (offset + frame_bytes > size_) {
    return absl::OutOfRangeError(absl::StrFormat(
        "WriteFrame overflow: offset (%zu) + frame_bytes (%zu) exceeds buffer "
        "size (%zu) in '%s'",
        offset, frame_bytes, size_, name_));
  }

  std::memcpy(data_ + offset, src, frame_bytes);
  return absl::OkStatus();
}

absl::Status ShmBuffer::ReadFrame(size_t frame_index, uint8_t* dest,
                                  size_t frame_bytes) const {
  if (dest == nullptr) {
    return absl::InvalidArgumentError("Destination frame pointer is null");
  }
  if (frame_bytes == 0) {
    return absl::InvalidArgumentError("frame_bytes must be positive");
  }

  size_t offset = frame_index * frame_bytes;
  if (offset + frame_bytes > size_) {
    return absl::OutOfRangeError(absl::StrFormat(
        "ReadFrame overflow: offset (%zu) + frame_bytes (%zu) exceeds buffer "
        "size (%zu) in '%s'",
        offset, frame_bytes, size_, name_));
  }

  std::memcpy(dest, data_ + offset, frame_bytes);
  return absl::OkStatus();
}

}  // namespace viduce::engine::interpolate
