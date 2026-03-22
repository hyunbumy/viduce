#include "engine/upscaler.h"

#include <memory>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/util.h"
#include "litert/c/litert_common.h"
#include "litert/cc/litert_common.h"
#include "litert/cc/litert_compiled_model.h"
#include "litert/cc/litert_environment.h"
#include "litert/cc/litert_macros.h"
#include "litert/cc/litert_model.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}  // extern "C"

namespace viduce::engine {

namespace {

constexpr int kUpscale = 4;

constexpr AVPixelFormat kModelIOFormat = AV_PIX_FMT_RGB24;
constexpr int kModelIONormScale = 255;

absl::Status Validate(Frame* frame) {
  if (frame == nullptr) {
    return absl::InvalidArgumentError("Input frame is null");
  }

  if (frame->stream_info().media_type != AVMEDIA_TYPE_VIDEO) {
    return absl::InvalidArgumentError("Input frame is not a video frame");
  }

  if (frame->frame()->format != AV_PIX_FMT_YUV420P) {
    return absl::InvalidArgumentError(
        "Input frame format is not YUV420P. Only YUV420P is supported for now");
  }

  return absl::OkStatus();
}

struct ModelIO {
  // Flattened pixel data in (ch, h, w) layout with normalized values in
  // [0.0, 1.0]
  std::vector<float> data;
};

absl::StatusOr<ModelIO> ToModelInput(Frame* frame) {
  AVFrame* av_frame = frame->frame();

  // Convert input frame to RGB format; for now do 8-bit
  // TODO: Consider supporting higher img depth
  std::unique_ptr<Frame> new_frame = Frame::Create({});
  AVFrame* rgb_frame = new_frame->frame();

  SwsContext* sws_scale_ctx = sws_getContext(
      av_frame->width, av_frame->height, (AVPixelFormat)av_frame->format,
      av_frame->width, av_frame->height, (AVPixelFormat)kModelIOFormat,
      SWS_FAST_BILINEAR,
      /*srcFilter=*/nullptr, /*dstFilter=*/nullptr, /*param=*/nullptr);
  absl::Cleanup free_sws_ctx = [&sws_scale_ctx]() {
    sws_freeContext(sws_scale_ctx);
  };
  if (sws_scale_ctx == nullptr) {
    return absl::InternalError(
        "Failed to create SwsContext for color conversion");
  }

  if (int res = sws_scale_frame(sws_scale_ctx, rgb_frame, av_frame); res < 0) {
    return absl::InternalError("Failed to scale frame with error: " +
                               AvErrToStr(res));
  }

  // Copy data into a vector in RGB: (ch, h, w) and normalize to [0.0f, 1.0f]
  // We can iterate through directly since:
  //   * Already in RGB
  //   * Packed so single plane in (ch, h, w)
  std::vector<float> normalized(rgb_frame->linesize[0]);
  for (int i = 0; i < normalized.size(); ++i) {
    normalized[i] = rgb_frame->data[0][i] / float(kModelIONormScale);
  }
  return ModelIO{.data = std::move(normalized)};
}

// Convert the model output back to an image.
absl::StatusOr<std::unique_ptr<Frame>> FromModelOutput(Frame* input_frame,
                                                       const ModelIO& output) {
  // Denormalize pixel values into RGB24
  std::vector<uint8_t> denormalized(output.data.size());
  for (int i = 0; i < denormalized.size(); ++i) {
    denormalized[i] = output.data[i] * kModelIONormScale;
  }

  AVFrame* input_avframe = input_frame->frame();
  // Convert pixels to a temp frame for sws_scale
  std::unique_ptr<Frame> temp_frame = Frame::Create({});
  AVFrame* temp_avframe = temp_frame->frame();
  temp_avframe->format = kModelIOFormat;
  temp_avframe->width = input_avframe->width * kUpscale;
  temp_avframe->height = input_avframe->height * kUpscale;
  int size = av_image_fill_arrays(
      temp_avframe->data, temp_avframe->linesize, denormalized.data(),
      kModelIOFormat, temp_avframe->width, temp_avframe->height, /*align=*/1);

  // Convert to a new frame in the same format as the input frame.
  SwsContext* sws_scale_ctx = sws_getContext(
      temp_avframe->width, temp_avframe->height,
      (AVPixelFormat)temp_avframe->format, temp_avframe->width,
      temp_avframe->height, (AVPixelFormat)input_avframe->format,
      SWS_FAST_BILINEAR,
      /*srcFilter=*/nullptr, /*dstFilter=*/nullptr, /*param=*/nullptr);
  absl::Cleanup free_sws_ctx = [&sws_scale_ctx]() {
    sws_freeContext(sws_scale_ctx);
  };
  if (sws_scale_ctx == nullptr) {
    return absl::InternalError(
        "Failed to create SwsContext for color conversion");
  }

  std::unique_ptr<Frame> new_frame = Frame::Create(input_frame->stream_info());
  AVFrame* new_avframe = new_frame->frame();
  if (int res = sws_scale_frame(sws_scale_ctx, new_avframe, temp_avframe);
      res < 0) {
    return absl::InternalError("Failed to scale frame with error: " +
                               AvErrToStr(res));
  }

  return std::move(new_frame);
}

// TODO: Extract into its separate class and receive as dependency
absl::StatusOr<litert::CompiledModel> GetModel() {
  // Initialize LiteRT environment
  LITERT_ASSIGN_OR_RETURN(auto env, litert::Environment::Create({}));

  // Compile the model for the CPU
  LITERT_ASSIGN_OR_RETURN(
      litert::CompiledModel compiled_model,
      litert::CompiledModel::Create(env, "models/output/realesrgan.tflite",
                                    litert::HwAccelerators::kCpu));
  return compiled_model;
}

// Runs the model and returns the output
absl::StatusOr<ModelIO> RunModel(litert::CompiledModel& model,
                                 const ModelIO& input) {
  LITERT_ASSIGN_OR_RETURN(std::vector<litert::TensorBuffer> input_buffers,
                          model.CreateInputBuffers());
  input_buffers[0].Write<float>(absl::MakeConstSpan(input.data));

  LITERT_ASSIGN_OR_RETURN(std::vector<litert::TensorBuffer> output_buffers,
                          model.CreateOutputBuffers());
  LITERT_RETURN_IF_ERROR(model.Run(input_buffers, output_buffers));

  LITERT_ASSIGN_OR_RETURN(size_t output_buffer_size, output_buffers[0].Size());
  size_t output_length = output_buffer_size / sizeof(float);
  std::vector<float> output(output_length);
  LITERT_RETURN_IF_ERROR(output_buffers[0].Read<float>(absl::MakeSpan(output)));

  // Normalize the model output to [0.0f, 1.0f] in case of outliers
  for (float& val : output) {
    val = std::min(std::max(val, 0.0f), 1.0f);
  }

  return ModelIO{.data = std::move(output)};
}

}  // namespace

absl::StatusOr<std::unique_ptr<Frame>> Upscaler::Upscale(Frame* input_frame) {
  if (absl::Status status = Validate(input_frame); status != absl::OkStatus()) {
    return status;
  }

  LITERT_ASSIGN_OR_RETURN(ModelIO input, ToModelInput(input_frame));

  LITERT_ASSIGN_OR_RETURN(litert::CompiledModel compiled_model, GetModel());

  LITERT_ASSIGN_OR_RETURN(ModelIO output, RunModel(compiled_model, input));

  LITERT_ASSIGN_OR_RETURN(std::unique_ptr<Frame> new_frame,
                          FromModelOutput(input_frame, output));

  return new_frame;
}

}  // namespace viduce::engine
