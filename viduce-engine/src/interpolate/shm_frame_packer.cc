#include "engine/interpolate/shm_frame_packer.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "engine/frame.h"
#include "engine/interpolate/shm_buffer.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}  // extern "C"

namespace viduce::engine::interpolate {

namespace {
constexpr int kRgbChannels = 3;
}  // namespace

ShmFramePacker::ShmFramePacker(int width, int height)
    : width_(width), height_(height) {}

ShmFramePacker::~ShmFramePacker() {
  if (sws_ctx_ != nullptr) {
    sws_freeContext(sws_ctx_);
  }
}

absl::Status ShmFramePacker::PackFrames(const std::vector<Frame*>& frames,
                                        ShmBuffer* buffer) {
  if (buffer == nullptr) {
    return absl::InvalidArgumentError("Destination ShmBuffer is null");
  }
  if (buffer->is_read_only()) {
    return absl::PermissionDeniedError(
        "Cannot pack frames into read-only ShmBuffer");
  }
  if (width_ <= 0 || height_ <= 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid packer dimensions: %dx%d", width_, height_));
  }
  if (frames.empty()) {
    return absl::InvalidArgumentError("Cannot pack empty frame list");
  }

  const size_t bytes_per_frame =
      static_cast<size_t>(width_) * static_cast<size_t>(height_) * kRgbChannels;
  const size_t total_bytes = frames.size() * bytes_per_frame;
  if (total_bytes > buffer->size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "ShmBuffer size (%zu bytes) is too small to pack %zu frames (%zu bytes "
        "required)",
        buffer->size(), frames.size(), total_bytes));
  }

  const int dst_stride[1] = {width_ * kRgbChannels};

  for (size_t i = 0; i < frames.size(); ++i) {
    Frame* frame = frames[i];
    if (frame == nullptr || frame->frame() == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Frame at index %zu is null", i));
    }
    if (frame->media_type() != AVMEDIA_TYPE_VIDEO) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Frame at index %zu is not a video frame", i));
    }

    AVFrame* av_frame = frame->frame();
    if (av_frame->width != width_ || av_frame->height != height_) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Frame at index %zu dimensions (%dx%d) do not match packer (%dx%d)",
          i, av_frame->width, av_frame->height, width_, height_));
    }
    if (av_frame->data[0] == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Frame at index %zu has unallocated pixel data", i));
    }

    const AVPixelFormat src_fmt = static_cast<AVPixelFormat>(av_frame->format);
    sws_ctx_ = sws_getCachedContext(
        sws_ctx_, width_, height_, src_fmt, width_, height_, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, /*srcFilter=*/nullptr, /*dstFilter=*/nullptr,
        /*param=*/nullptr);
    if (sws_ctx_ == nullptr) {
      return absl::InternalError(
          "Failed to initialize SwsContext for frame packing");
    }

    uint8_t* dst_slice[1] = {buffer->data() + i * bytes_per_frame};
    int scaled_h = sws_scale(sws_ctx_, av_frame->data, av_frame->linesize, 0,
                             height_, dst_slice, dst_stride);
    if (scaled_h != height_) {
      return absl::InternalError(absl::StrFormat(
          "sws_scale failed at frame %zu: expected %d rows, got %d", i, height_,
          scaled_h));
    }
  }

  return absl::OkStatus();
}

}  // namespace viduce::engine::interpolate
