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

  virtual absl::StatusOr<std::vector<float>> RunModel(
      const std::vector<float>& input) = 0;
};

}  // namespace viduce::engine::upscale
