#include "engine/interpolate/shm_frame_unpacker.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "engine/frame.h"
#include "engine/interpolate/shm_buffer.h"
#include "engine/media_info.h"
#include "engine/util.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}  // extern "C"

namespace viduce::engine::interpolate {

namespace {
constexpr int kRgbChannels = 3;
using ::viduce::engine::util::AvErrToStr;
}  // namespace

ShmFrameUnpacker::ShmFrameUnpacker(int width, int height,
                                   StreamIndex stream_index,
                                   AVPixelFormat output_pix_fmt)
    : width_(width),
      height_(height),
      stream_index_(stream_index),
      output_pix_fmt_(output_pix_fmt) {}

ShmFrameUnpacker::~ShmFrameUnpacker() {
  if (sws_ctx_ != nullptr) {
    sws_freeContext(sws_ctx_);
  }
}

absl::StatusOr<std::vector<std::unique_ptr<Frame>>>
ShmFrameUnpacker::UnpackFrames(const ShmBuffer& buffer, size_t num_frames) {
  if (width_ <= 0 || height_ <= 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid unpacker dimensions: %dx%d", width_, height_));
  }
  if (num_frames == 0) {
    return absl::InvalidArgumentError("num_frames must be positive");
  }

  const size_t bytes_per_frame =
      static_cast<size_t>(width_) * static_cast<size_t>(height_) * kRgbChannels;
  const size_t total_bytes = num_frames * bytes_per_frame;
  if (total_bytes > buffer.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "ShmBuffer size (%zu bytes) is smaller than requested %zu frames (%zu "
        "bytes)",
        buffer.size(), num_frames, total_bytes));
  }

  sws_ctx_ = sws_getCachedContext(
      sws_ctx_, width_, height_, AV_PIX_FMT_RGB24, width_, height_,
      output_pix_fmt_, SWS_BILINEAR, /*srcFilter=*/nullptr,
      /*dstFilter=*/nullptr, /*param=*/nullptr);
  if (sws_ctx_ == nullptr) {
    return absl::InternalError(
        "Failed to initialize SwsContext for frame unpacking");
  }

  const int src_stride[1] = {width_ * kRgbChannels};
  std::vector<std::unique_ptr<Frame>> output;
  output.reserve(num_frames);

  for (size_t i = 0; i < num_frames; ++i) {
    absl::StatusOr<std::unique_ptr<Frame>> frame =
        Frame::Create(stream_index_, AVMEDIA_TYPE_VIDEO);
    if (!frame.ok()) {
      return frame.status();
    }

    AVFrame* av_frame = (*frame)->frame();
    av_frame->width = width_;
    av_frame->height = height_;
    av_frame->format = output_pix_fmt_;
    av_frame->pts = static_cast<int64_t>(i);

    int buf_res = av_frame_get_buffer(av_frame, /*align=*/0);
    if (buf_res < 0) {
      return absl::InternalError(absl::StrFormat(
          "Failed to allocate AVFrame buffer at index %zu: %s", i,
          AvErrToStr(buf_res)));
    }

    const uint8_t* src_slice[1] = {buffer.data() + i * bytes_per_frame};
    int scaled_h = sws_scale(sws_ctx_, src_slice, src_stride, 0, height_,
                             av_frame->data, av_frame->linesize);
    if (scaled_h != height_) {
      return absl::InternalError(absl::StrFormat(
          "sws_scale failed unpacking frame %zu: expected %d rows, got %d", i,
          height_, scaled_h));
    }

    output.push_back(std::move(*frame));
  }

  return output;
}

}  // namespace viduce::engine::interpolate
