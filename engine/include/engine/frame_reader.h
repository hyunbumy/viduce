#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "engine/frame.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}  // extern "C"

namespace viduce::engine {

class FrameReader {
 public:
  static absl::StatusOr<std::unique_ptr<FrameReader>> Create(
      std::string_view url);

  ~FrameReader();

  // Returns the next available frame or null if EOF.
  absl::StatusOr<std::unique_ptr<Frame>> ReadNextFrame();

 private:
  explicit FrameReader(AVFormatContext* format_ctx,
                       std::vector<AVCodecContext*> codecs)
      : format_ctx_(format_ctx), codecs_(codecs) {}

  AVFormatContext* format_ctx_;
  std::vector<AVCodecContext*> codecs_;
};

}  // namespace viduce::engine
