#pragma once

#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "engine/upscale/model.h"
#include "litert/cc/litert_compiled_model.h"
#include "litert/cc/litert_environment.h"

namespace viduce::engine::upscale {

class ModelImpl : public Model {
 public:
  static absl::StatusOr<ModelImpl> Create(std::string_view model_path);

  Info getInfo() override;

  // For now, the model is hardcoded to support 240x240 dimension input.
  absl::StatusOr<ModelIo> RunModel(const ModelIo& input) override;

 private:
  explicit ModelImpl(litert::Environment& env, litert::CompiledModel& model);

  litert::Environment env_;
  litert::CompiledModel model_;
};

}  // namespace viduce::engine::upscale