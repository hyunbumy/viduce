#include "engine/engine_api.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame_reader.h"
#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavutil/avutil.h>
}  // extern "C"

namespace {

// TODO: Migrate these functionalities into a separate class for better
// testability
absl::Status DecodeVideoInternal(const char* input_path) {
  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(input_path);
  if (!frame_reader.ok()) {
    return frame_reader.status();
  }

  while (true) {
    absl::StatusOr<std::unique_ptr<viduce::engine::Frame>> frame =
        (*frame_reader)->ReadNextFrame();
    if (!frame.ok()) {
      return frame.status();
    }

    if (*frame == nullptr) {
      // Done reading
      break;
    }

    viduce::engine::Frame::StreamInfo stream_info = (*frame)->stream_info();
    AVFrame* av_frame = (*frame)->frame();
    spdlog::info(
        "Decoded frame info: res ({}x{}), stream_index: {}, media_type: {}, "
        "codec_id: {}",
        av_frame->width, av_frame->height, stream_info.stream_index,
        av_get_media_type_string(stream_info.media_type),
        avcodec_get_name(stream_info.codec_id));
  }

  return absl::OkStatus();
}

}  // namespace

int DecodeVideo(const char* input_path) {
  absl::Status status = DecodeVideoInternal(input_path);
  if (!status.ok()) {
    spdlog::error("DecodeVideo failed: {}", status.message());
  }
  return status.raw_code();
}
