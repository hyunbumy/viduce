#pragma once

#include <memory>

#include "absl/status/statusor.h"
#include "engine/media_info.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}  // extern "C"

namespace viduce::engine {

class Frame {
 public:
  static absl::StatusOr<std::unique_ptr<Frame>> Create(
      const StreamInfo& stream_info);

  ~Frame();

  AVFrame* frame() { return frame_; }

  // The original stream info that this frame was decoded from.
  const StreamInfo& stream_info() const { return stream_info_; }

 private:
  explicit Frame(const StreamInfo& stream_info, AVFrame* frame)
      : stream_info_(stream_info), frame_(frame) {}

  AVFrame* frame_;
  const StreamInfo stream_info_;
};

}  // namespace viduce::engine
