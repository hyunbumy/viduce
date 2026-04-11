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
  // Creates a new Frame with the given stream info. The underlying AVFrame will
  // NOT be populated with any data.
  static absl::StatusOr<std::unique_ptr<Frame>> Create(
      const StreamInfo& stream_info);

  // Clones a Frame into a new Frame with the same stream info and AVFrame
  // metadata. The underlying AVFrame data buffers will NOT be copied.
  static absl::StatusOr<std::unique_ptr<Frame>> Clone(Frame* frame);

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
