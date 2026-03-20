#include "engine/upscaler.h"

#include <memory>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>

#include "absl/status/statusor.h"
#include "litert/c/litert_common.h"
#include "litert/cc/litert_common.h"
#include "litert/cc/litert_compiled_model.h"
#include "litert/cc/litert_environment.h"
#include "litert/cc/litert_macros.h"
#include "litert/cc/litert_model.h"

namespace viduce::engine {

namespace {

constexpr float kBit8ScaleFactor = 1.0f / 255.0f;
constexpr int kScale = 4;

}  // namespace

absl::StatusOr<std::unique_ptr<ImageFrame>> Upscaler::Upscale(
    const ImageFrame* input_img) {
  // TODO: Convert input_img to cv::Mat
  cv::Mat input;

  // Convert to format that the model expects: RGB, float32, normalized to
  // [0.0f, 1.0f], and in (ch, h, w) layout.
  // For now, assume 8-bit BGR input.
  cv::Mat rgb_float_chw =
      cv::dnn::blobFromImage(input, kBit8ScaleFactor, cv::Size(), cv::Scalar(),
                             /*swapRB=*/true, /*crop=*/false, CV_32F);

  std::cout << "input img total: " << rgb_float_chw.total() << std::endl;

  // Initialize LiteRT environment
  LITERT_ASSIGN_OR_ABORT(auto env, litert::Environment::Create({}));

  // Compile the model for the CPU
  LITERT_ASSIGN_OR_ABORT(
      auto compiled_model,
      litert::CompiledModel::Create(env, "models/output/realesrgan.tflite",
                                    litert::HwAccelerators::kCpu));

  LITERT_ASSIGN_OR_ABORT(auto input_buffers,
                         compiled_model.CreateInputBuffers());
  std::cout << "Input buffers size: " << input_buffers.size() << std::endl;
  LITERT_ASSIGN_OR_ABORT(auto input_req,
                         compiled_model.GetInputBufferRequirements(0));
  LITERT_ASSIGN_OR_ABORT(auto buf_size, input_req.BufferSize());
  // With an example input of (ch=3, h=1, w=1) of type float32, the buf_size is
  // 3 elem with 12 bytes (3 * 4 bytes for float32)
  std::cout << "Input buffer size: " << buf_size << std::endl;

  input_buffers[0].Write<float>(
      absl::MakeConstSpan(rgb_float_chw.ptr<float>(),
                          rgb_float_chw.total() * rgb_float_chw.channels()));

  LITERT_ASSIGN_OR_RETURN(auto output_buffers,
                          compiled_model.CreateOutputBuffers());
  std::cout << "Output buffers size: " << output_buffers.size() << std::endl;
  LITERT_ASSIGN_OR_ABORT(auto output_req,
                         compiled_model.GetOutputBufferRequirements(0));
  LITERT_ASSIGN_OR_ABORT(auto out_buf_size, output_req.BufferSize());
  // With an example input of (ch=3, h=1, w=1) of type float32 and scale of 4,
  // the out_buf_size is 48 elem with 192 bytes (48 * 4 bytes for float32)
  std::cout << "Output buffer size: " << out_buf_size << std::endl;

  LITERT_RETURN_IF_ERROR(compiled_model.Run(input_buffers, output_buffers));

  // Convert the output weight back to an image.
  // Output of the model should be a weight ranging [0.0f, 1.0f] for each pixel
  // in the input image.
  // Output layout: RGB and (batch=1, ch, h, w)
  std::vector<float> output(out_buf_size / sizeof(float));
  LITERT_RETURN_IF_ERROR(output_buffers[0].Read<float>(absl::MakeSpan(output)));

  // Construct the output image.
  // Denormalize the pixel values
  for (float& val : output) {
    val = std::min(std::max(val, 0.0f), 1.0f) * 255.0f;
  }
  // Convert to image
  int batch = 1;
  int ch = 3;
  int h = input.rows * kScale;
  int w = input.cols * kScale;
  std::vector<int> sizes{batch, ch, h, w};
  cv::Mat out_rgb_float_chw(
      /*ndims=*/sizes.size(), sizes.data(), CV_32F, (void*)output.data());
  std::vector<cv::Mat> output_imgs;
  cv::dnn::imagesFromBlob(out_rgb_float_chw, output_imgs);
  std::cout << "Output imgs size: " << output_imgs.size() << std::endl;

  return std::make_unique<ImageFrame>();
}

}  // namespace viduce::engine
