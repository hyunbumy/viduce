#pragma once

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/upscale/model.h"

namespace viduce::engine::upscale {

class Upscaler {
 public:
  explicit Upscaler(std::unique_ptr<Model> model);

  // Returns an upscaled version of the input image frame.
  absl::StatusOr<std::unique_ptr<Frame>> Upscale(Frame* input_frame);

 private:
  absl::StatusOr<std::vector<float>> ToModelInput(Frame* input);
  absl::StatusOr<std::unique_ptr<Frame>> FromModelOutput(
      Frame* input, const std::vector<float>& output);

  std::unique_ptr<Model> model_;
};

}  // namespace viduce::engine::upscale
