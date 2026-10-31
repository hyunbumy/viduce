#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/interpolate/shm_buffer.h"
#include "engine/media_info.h"

extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}  // extern "C"

namespace viduce::engine::interpolate {

class ShmFrameUnpacker {
 public:
  ShmFrameUnpacker(int width, int height,
                   StreamIndex stream_index = StreamIndex{0},
                   AVPixelFormat output_pix_fmt = AV_PIX_FMT_RGB24);
  ~ShmFrameUnpacker();

  absl::StatusOr<std::vector<std::unique_ptr<Frame>>> UnpackFrames(
      const ShmBuffer& buffer, size_t num_frames);

 private:
  int width_;
  int height_;
  StreamIndex stream_index_;
  AVPixelFormat output_pix_fmt_;
  SwsContext* sws_ctx_ = nullptr;
};

}  // namespace viduce::engine::interpolate
