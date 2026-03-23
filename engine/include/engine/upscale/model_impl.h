#pragma once

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "engine/upscale/model.h"
#include "litert/cc/litert_compiled_model.h"
#include "litert/cc/litert_environment.h"

namespace viduce::engine::upscale {

class ModelImpl : public Model {
 public:
  static absl::StatusOr<ModelImpl> Create(std::string model_path);

  Info getInfo() override;

  absl::StatusOr<std::vector<float>> RunModel(
      const std::vector<float>& input) override;

 private:
  explicit ModelImpl(litert::Environment& env, litert::CompiledModel& model);

  litert::Environment env_;
  litert::CompiledModel model_;
};

}  // namespace viduce::engine::upscale