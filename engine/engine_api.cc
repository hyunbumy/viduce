#include "engine_api.h"

#include "spdlog/spdlog.h"

extern "C" {
#include <libavformat/avformat.h>
}  // extern "C"

int decode_video(const char* input_path, const char* output_dir) {
  AVFormatContext* format_ctx = avformat_alloc_context();
  int open_res = avformat_open_input(&format_ctx, input_path, nullptr, nullptr);
  if (open_res != 0) {
    spdlog::error("Failed to open input video: {} with error code: {}",
                  input_path, open_res);
    return -1;
  }
  return 0;
}
