#include "engine/frame.h"

#include <memory>

#include "absl/status/statusor.h"
#include "engine/media_info.h"

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

absl::StatusOr<std::unique_ptr<Frame>> Frame::Clone(Frame* frame) {
  StreamInfo new_stream_info(frame->stream_info());

  AVFrame* new_avframe = av_frame_alloc();
  if (frame == nullptr) {
    return absl::InternalError("Failed to allocate av_frame");
  }
  av_frame_copy_props(new_avframe, frame->frame());

  return std::unique_ptr<Frame>(new Frame(new_stream_info, new_avframe));
}

Frame::~Frame() { av_frame_free(&frame_); }

}  // namespace viduce::engine
