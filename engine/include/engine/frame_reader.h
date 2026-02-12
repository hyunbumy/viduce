#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}  // extern "C"

namespace viduce::engine {

class Frame {
 public:
  // Extra information about the stream that the frame belongs to.
  struct StreamInfo {
    int stream_index = -1;
    AVMediaType media_type = AVMEDIA_TYPE_UNKNOWN;
    AVCodecID codec_id = AV_CODEC_ID_NONE;
  };

  static std::unique_ptr<Frame> Create(AVStream* stream);

  ~Frame() { av_frame_free(&frame_); }

  AVFrame* frame() { return frame_; }

  const StreamInfo& stream_info() const { return stream_info_; }

 private:
  explicit Frame(const StreamInfo& stream_info) : stream_info_(stream_info) {
    frame_ = av_frame_alloc();
  }

  AVFrame* frame_;
  const StreamInfo stream_info_;
};

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
