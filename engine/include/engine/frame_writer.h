#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>

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
  // Parameters for the incoming payloads
  struct InputParams {
    std::vector<StreamInfo> streams;
  };

  // Parameters for configuring the output
  struct OutputParams {
    std::string url;
  };

  struct EncoderInfo {
    AVStream* encoding_stream;
    AVCodecContext* encoder_ctx;
  };

  static absl::StatusOr<std::unique_ptr<FrameWriter>> Create(
      const InputParams& input_params, const OutputParams& output_params);

  ~FrameWriter();

  // Writes a frame into the output video. Does not guarantee that the frame is
  // flushed into the output video yet until Flush is called.
  absl::Status Write(std::unique_ptr<Frame> frame);

  // Flushes all the frames into an output video.
  absl::Status Flush();

 private:
  explicit FrameWriter(
      AVFormatContext* fmt_ctx, AVPacket* packet,
      const std::unordered_map<StreamIndex, EncoderInfo>& encoders);

  absl::Status EncodeFrame(StreamIndex stream_index, AVFrame* frame);

  AVFormatContext* format_ctx_;
  std::unordered_map<StreamIndex, EncoderInfo> encoders_;
  AVPacket* packet_;
};

}  // namespace viduce::engine
