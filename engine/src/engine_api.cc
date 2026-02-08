#include "engine/engine_api.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame_reader.h"
#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
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

    AVFrame* av_frame = (*frame)->get_frame();
    spdlog::info("Decoded frame info: res ({%d}x{%d}", av_frame->width,
                 av_frame->height);
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
