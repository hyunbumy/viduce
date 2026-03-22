#include "engine/frame.h"

#include <memory>

#include "absl/status/statusor.h"

extern "C" {
#include <libavutil/avutil.h>
}  // extern "C"

namespace viduce::engine {

absl::StatusOr<std::unique_ptr<Frame>> Frame::Create(
    const StreamInfo& stream_info) {
  AVFrame* frame = av_frame_alloc();
  if (frame == nullptr) {
    return absl::InternalError("Failed to allocate av_frame");
  }
  return std::unique_ptr<Frame>(new Frame(stream_info, frame));
}

Frame::~Frame() { av_frame_free(&frame_); }

}  // namespace viduce::engine
