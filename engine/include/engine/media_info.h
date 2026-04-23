#pragma once

#include <variant>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
}  // extern "C"

namespace viduce::engine {

struct VideoInfo {
  // Video dimension in pixels
  struct Dimension {
    int width = 0;
    int height = 0;
  };
  Dimension dim = {};

};

struct AudioInfo {};

struct StreamInfo {
  int stream_index = -1;
  int64_t num_frames = 0;

  AVCodecID codec_id = AV_CODEC_ID_NONE;

  AVRational time_base = {};
  // Duration of the stream in the given timebase
  int64_t duration = 0;

  std::variant<std::monostate, VideoInfo, AudioInfo> type_info;
};

struct MediaInfo {
  // Ordered by their StreamInfo.stream_index
  std::vector<StreamInfo> streams = {};
};

}  // namespace viduce::engine
