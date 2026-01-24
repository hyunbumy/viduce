#include "engine/engine_api.h"

#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}  // extern "C"

namespace {

std::string AvErrToStr(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
  return std::string(av_make_error_string(errbuf, sizeof(errbuf), errnum));
}

// TODO: Migrate these functionalities into a separate class for better
// testability
absl::Status DecodeVideoInternal(const char* input_path) {
  AVFormatContext* format_ctx = avformat_alloc_context();
  int open_res = avformat_open_input(&format_ctx, input_path, nullptr, nullptr);
  if (open_res != 0) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Failed to open input video file %s with error: %s",
                        input_path, AvErrToStr(open_res)));
  }

  // Read frames
  // av_read_frame will be responsible for reaching each frames which will block
  // until it's ready.
  // If input is being "streamed" in, then we must not block the thread:
  // https://stackoverflow.com/questions/23800615/ffmpeg-av-open-input-stream-in-latest-ffmpeg
  AVPacket* packet = av_packet_alloc();
  while (true) {
    int read_res = av_read_frame(format_ctx, packet);
    if (read_res == AVERROR_EOF) {
      // Done processing
      break;
    }

    if (read_res != 0) {
      return absl::Status(absl::StatusCode::kInternal,
                          absl::StrFormat("Error reading frame with error: %s",
                                          AvErrToStr(read_res)));
    }

    // TODO: Process the packet
    spdlog::info("Read packet with size: {}", packet->size);
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
