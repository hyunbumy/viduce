#include "engine/upscale/upscaler.h"

#include <memory>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/util.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}  // extern "C"

namespace viduce::engine::upscale {

namespace {

constexpr AVPixelFormat kModelIOFormat = AV_PIX_FMT_RGB24;
constexpr int kModelNormScale = 255;

absl::Status Validate(Frame* frame) {
  if (frame == nullptr) {
    return absl::InvalidArgumentError("Input frame is null");
  }

  if (frame->stream_info().media_type != AVMEDIA_TYPE_VIDEO) {
    return absl::InvalidArgumentError("Input frame is not a video frame");
  }

  return absl::OkStatus();
}

struct ConversionSpec {
  int width;
  int height;
  AVPixelFormat pix_fmt;
};

absl::StatusOr<std::unique_ptr<Frame>> ConvertColor(
    Frame* input, const ConversionSpec& spec) {
  SwsContext* sws_scale_ctx = sws_getContext(
      input->frame()->width, input->frame()->height,
      (AVPixelFormat)input->frame()->format, spec.width, spec.height,
      spec.pix_fmt, SWS_FAST_BILINEAR,
      /*srcFilter=*/nullptr, /*dstFilter=*/nullptr, /*param=*/nullptr);
  absl::Cleanup ctx_cleanup = [&sws_scale_ctx]() {
    sws_freeContext(sws_scale_ctx);
  };

  if (sws_scale_ctx == nullptr) {
    return absl::InternalError("Failed to create SwsContext");
  }

  absl::StatusOr<std::unique_ptr<Frame>> dst_frame = Frame::Create({});
  if (!dst_frame.ok()) {
    return dst_frame.status();
  }
  AVFrame* dst_avframe = (*dst_frame)->frame();

  if (int res = sws_scale_frame(sws_scale_ctx, dst_avframe, input->frame());
      res < 0) {
    return absl::InternalError("Failed to scale frame with error: " +
                               AvErrToStr(res));
  }

  return std::move(*dst_frame);
}

}  // namespace

Upscaler::Upscaler(Model* model) : model_(model) {}

absl::StatusOr<std::unique_ptr<Frame>> Upscaler::Upscale(Frame* input_frame) {
  if (absl::Status status = Validate(input_frame); status != absl::OkStatus()) {
    return status;
  }

  absl::StatusOr<Model::ModelIo> input = ToModelInput(input_frame);
  if (!input.ok()) {
    return input.status();
  }

  absl::StatusOr<Model::ModelIo> output = model_->RunModel(*input);
  if (!output.ok()) {
    return output.status();
  }

  return FromModelOutput(input_frame, *output);
}

absl::StatusOr<Model::ModelIo> Upscaler::ToModelInput(Frame* frame) {
  // Convert input frame to RGB format; for now do 8-bit
  // TODO: Consider supporting higher img depth
  absl::StatusOr<std::unique_ptr<Frame>> rgb_frame =
      ConvertColor(frame, {.width = frame->frame()->width,
                           .height = frame->frame()->height,
                           .pix_fmt = kModelIOFormat});
  if (!rgb_frame.ok()) {
    return rgb_frame.status();
  }
  AVFrame* rgb_avframe = (*rgb_frame)->frame();

  // Copy data into a vector in RGB: (ch, h, w) and normalize to [0.0f, 1.0f]
  // We can iterate through directly since:
  //   * Already in RGB
  //   * Packed so single plane in (ch, h, w)
  std::vector<float> normalized(rgb_avframe->linesize[0]);
  for (int i = 0; i < normalized.size(); ++i) {
    normalized[i] = rgb_avframe->data[0][i] / float(kModelNormScale);
  }
  return Model::ModelIo{.data = std::move(normalized)};
}

// Convert the model output back to an image.
absl::StatusOr<std::unique_ptr<Frame>> Upscaler::FromModelOutput(
    Frame* input_frame, const Model::ModelIo& output) {
  // Denormalize pixel values into RGB24
  std::vector<uint8_t> denormalized(output.data.size());
  for (int i = 0; i < denormalized.size(); ++i) {
    denormalized[i] = output.data[i] * kModelNormScale;
  }

  AVFrame* input_avframe = input_frame->frame();
  // Convert pixels to a temp frame for sws_scale
  absl::StatusOr<std::unique_ptr<Frame>> temp_frame = Frame::Create({});
  AVFrame* temp_avframe = (*temp_frame)->frame();
  temp_avframe->format = kModelIOFormat;
  temp_avframe->width = input_avframe->width * model_->getInfo().scale;
  temp_avframe->height = input_avframe->height * model_->getInfo().scale;
  int size = av_image_fill_arrays(
      temp_avframe->data, temp_avframe->linesize, denormalized.data(),
      kModelIOFormat, temp_avframe->width, temp_avframe->height, /*align=*/1);
  if (size < 0) {
    return absl::InternalError("Failed to create temp output RGB Frame: " +
                               AvErrToStr(size));
  }

  // Convert to a new frame in the same format as the input frame.
  return ConvertColor(temp_frame->get(),
                      {.width = temp_avframe->width,
                       .height = temp_avframe->height,
                       .pix_fmt = (AVPixelFormat)input_avframe->format});
}

}  // namespace viduce::engine::upscale
