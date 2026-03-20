#pragma once

#include <memory>

#include "absl/status/statusor.h"

namespace viduce::engine {

struct ImageFrame {};

class Upscaler {
 public:
  // Returns an upscaled version of the input image frame.
  absl::StatusOr<std::unique_ptr<ImageFrame>> Upscale(
      const ImageFrame* input_img);
};

}  // namespace viduce::engine
