#include "engine/upscale/model_impl.h"

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "engine/upscale/model.h"
#include "litert/c/litert_common.h"
#include "litert/cc/litert_common.h"
#include "litert/cc/litert_compiled_model.h"
#include "litert/cc/litert_environment.h"
#include "litert/cc/litert_macros.h"
#include "litert/cc/litert_model.h"

extern "C" {
#include <libavutil/avutil.h>
}  // extern "C"

namespace viduce::engine::upscale {

namespace {

constexpr int kUpscale = 4;

}  // namespace

ModelImpl::ModelImpl(litert::Environment& env, litert::CompiledModel& model)
    : env_(std::move(env)), model_(std::move(model)) {}

absl::StatusOr<ModelImpl> ModelImpl::Create(std::string_view model_path) {
  // Initialize LiteRT environment
  LITERT_ASSIGN_OR_RETURN(litert::Environment env,
                          litert::Environment::Create({}));

  // Compile the model for the CPU
  LITERT_ASSIGN_OR_RETURN(
      litert::CompiledModel compiled_model,
      litert::CompiledModel::Create(env, std::string(model_path),
                                    litert::HwAccelerators::kCpu));

  return ModelImpl(env, compiled_model);
}

Model::Info ModelImpl::getInfo() { return Model::Info{.scale = kUpscale}; }

absl::StatusOr<Model::ModelIo> ModelImpl::RunModel(
    const Model::ModelIo& input) {
  LITERT_ASSIGN_OR_RETURN(std::vector<litert::TensorBuffer> input_buffers,
                          model_.CreateInputBuffers());
  input_buffers[0].Write<float>(absl::MakeConstSpan(input.data));

  LITERT_ASSIGN_OR_RETURN(std::vector<litert::TensorBuffer> output_buffers,
                          model_.CreateOutputBuffers());

  LITERT_RETURN_IF_ERROR(model_.Run(input_buffers, output_buffers));

  LITERT_ASSIGN_OR_RETURN(size_t output_buffer_size, output_buffers[0].Size());
  size_t output_length = output_buffer_size / sizeof(float);
  std::vector<float> output(output_length);
  LITERT_RETURN_IF_ERROR(output_buffers[0].Read<float>(absl::MakeSpan(output)));

  // Normalize the model output to [0.0f, 1.0f] in case of outliers
  for (float& val : output) {
    val = std::min(std::max(val, 0.0f), 1.0f);
  }

  return ModelIo{.data = std::move(output)};
}

}  // namespace viduce::engine::upscale