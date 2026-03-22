#pragma once

#include <memory>

#include "absl/status/statusor.h"
#include "engine/frame.h"

namespace viduce::engine {

class Upscaler {
 public:
  // Returns an upscaled version of the input image frame.
  absl::StatusOr<std::unique_ptr<Frame>> Upscale(Frame* input_frame);
};

}  // namespace viduce::engine
