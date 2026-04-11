#pragma once

#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}  // extern "C"

namespace viduce::engine {

using StreamIndex = int;

class FrameWriter {
 public:
  struct EncoderInfo {
    AVStream* encoding_stream;
    AVCodecContext* encoder_ctx;
  };

  static absl::StatusOr<std::unique_ptr<FrameWriter>> Create(
      std::string_view output_url);

  ~FrameWriter();

  // Writes a frame into the output video. Does not guarantee that the frame is
  // flushed into the output video yet until Flush is called.
  absl::Status Write(std::unique_ptr<Frame> frame);

  // Flushes all the frames into an output video.
  absl::Status Flush();

 private:
  FrameWriter() = default;

  AVFormatContext* format_ctx_;
  std::unordered_map<StreamIndex, EncoderInfo> encoders_;
  AVPacket* packet_;
};

}  // namespace viduce::engine
