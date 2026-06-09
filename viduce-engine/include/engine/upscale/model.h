#pragma once

#include <vector>

#include "absl/status/statusor.h"

extern "C" {
#include <libavutil/avutil.h>
}  // extern "C"

namespace viduce::engine::upscale {

class Model {
 public:
  struct Info {
    int scale;
  };
  virtual Info getInfo() = 0;

  struct ModelIo {
    // 1-D array of normalized RBG pixel data in the range of [0.0f, 1.0f] with
    // a layout of (channel, height, width).
    std::vector<float> data;
  };
  virtual absl::StatusOr<ModelIo> RunModel(const ModelIo& input) = 0;
};

}  // namespace viduce::engine::upscale
