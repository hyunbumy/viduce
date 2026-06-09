#pragma once

#include <variant>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
}  // extern "C"

namespace viduce::engine {

// Strongly-typed alias for stream indices. Prevents accidental mixing with
// arbitrary ints (e.g., array sizes, codec IDs). Convert with static_cast or
// brace-init: `StreamIndex{packet->stream_index}` /
// `static_cast<int>(stream_index)`.
enum class StreamIndex : int {};

struct VideoInfo {
  // Video dimension in pixels
  struct Dimension {
    int width = 0;
    int height = 0;
  };
  Dimension dim = {};

  AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
};

struct AudioInfo {};

struct StreamInfo {
  StreamIndex stream_index = StreamIndex{-1};
  int64_t num_frames = 0;

  AVCodecID codec_id = AV_CODEC_ID_NONE;

  AVRational time_base = {};
  // Duration of the stream in the given timebase
  int64_t duration = 0;

  std::variant<std::monostate, VideoInfo, AudioInfo> type_info;
};

struct MediaInfo {
  // Ordered by their StreamInfo.stream_index — i.e., callers rely on
  // streams[i].stream_index == StreamIndex{i}.
  // TODO: Refactor to std::unordered_map<StreamIndex, StreamInfo> so the
  // index-to-stream relationship is explicit at the type level instead of
  // depending on vector ordering.
  std::vector<StreamInfo> streams = {};
};

}  // namespace viduce::engine
