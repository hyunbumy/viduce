#pragma once

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/media_info.h"

extern "C" {
#include <libavutil/pixfmt.h>
}  // extern "C"

namespace viduce::engine::interpolate {

struct InterpolationConfig {
  int width = 0;
  int height = 0;
  int temporal_scale_factor = 4;
  int sampling_steps = 16;
  StreamIndex stream_index = StreamIndex{0};
  AVPixelFormat output_pix_fmt = AV_PIX_FMT_RGB24;
};

class Interpolator {
 public:
  virtual ~Interpolator() = default;

  virtual absl::Status Init(const InterpolationConfig& config) = 0;

  virtual absl::StatusOr<std::vector<std::unique_ptr<Frame>>> InterpolateChunk(
      const std::vector<Frame*>& input_frames, bool is_last_chunk) = 0;

  virtual absl::Status Close() = 0;
};

}  // namespace viduce::engine::interpolate
