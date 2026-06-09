#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/media_info.h"

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

  FrameReader(const FrameReader&) = delete;
  FrameReader& operator=(const FrameReader&) = delete;
  FrameReader(FrameReader&&) = delete;
  FrameReader& operator=(FrameReader&&) = delete;

  const MediaInfo& media_info() const { return media_info_; }

  // Returns the next available frame or null if EOF.
  // Return order should be based on presentation timestamp.
  absl::StatusOr<std::unique_ptr<Frame>> ReadNextFrame();

 private:
  explicit FrameReader(AVFormatContext* format_ctx, AVPacket* packet,
                       const std::vector<AVCodecContext*>& codecs,
                       const MediaInfo& media_info)
      : format_ctx_(format_ctx),
        packet_(packet),
        codecs_(codecs),
        media_info_(media_info) {}

  AVFormatContext* format_ctx_;
  AVPacket* packet_;
  std::vector<AVCodecContext*> codecs_;

  MediaInfo media_info_;
};

}  // namespace viduce::engine
