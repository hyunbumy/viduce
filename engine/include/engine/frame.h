#pragma once

#include <memory>

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

  static std::unique_ptr<Frame> Create(AVStream* stream) {
    return Frame::Create(StreamInfo{.stream_index = stream->index,
                                    .media_type = stream->codecpar->codec_type,
                                    .codec_id = stream->codecpar->codec_id});
  }

  static std::unique_ptr<Frame> Create(const StreamInfo& stream_info) {
    AVFrame* frame = av_frame_alloc();
    return std::make_unique<Frame>(stream_info);
  }

  ~Frame() { av_frame_free(&frame_); }

  AVFrame* frame() { return frame_; }

  const StreamInfo& stream_info() const { return stream_info_; }

 private:
  explicit Frame(const StreamInfo& stream_info, AVFrame* frame)
      : stream_info_(stream_info), frame_(frame) {}

  AVFrame* frame_;
  const StreamInfo stream_info_;
};

}  // namespace viduce::engine
