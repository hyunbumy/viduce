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

using ::viduce::engine::util::AvErrToStr;

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

class Gbr24pConverter {
 public:
  absl::StatusOr<Model::ModelIo> ToModelIo(Frame* frame) {
    // Convert input frame to RGB format
    absl::StatusOr<std::unique_ptr<Frame>> rgb_frame =
        ConvertColor(frame, {.width = frame->frame()->width,
                             .height = frame->frame()->height,
                             .pix_fmt = conv_format_});
    if (!rgb_frame.ok()) {
      return rgb_frame.status();
    }
    AVFrame* rgb_avframe = (*rgb_frame)->frame();

    // Copy data into a vector of RGB in (ch, h, w) and normalize to
    // [0.0f, 1.0f]
    std::vector<float> normalized;
    normalized.reserve(rgb_avframe->width * rgb_avframe->height * 3);
    for (int ch = 0; ch < 3; ++ch) {
      for (int h = 0; h < rgb_avframe->height; ++h) {
        for (int w = 0; w < rgb_avframe->width; ++w) {
          // Use the original unpadded linesize
          // We iterate only until the width to address potential padding
          int orig_ind = w + h * rgb_avframe->linesize[ch];
          float norm_data =
              rgb_avframe->data[ch][orig_ind] / float(norm_scale_);
          normalized.push_back(norm_data);
        }
      }
    }

    return Model::ModelIo{.data = std::move(normalized)};
  }

  absl::StatusOr<std::unique_ptr<Frame>> FromModelIo(
      Frame* input_frame, const Model::ModelIo& output, int out_scale) {
    // Denormalize pixel values from RGB in (ch, h, w) to GBR24P
    std::vector<uint8_t> denormalized(output.data.size());
    for (int i = 0; i < denormalized.size(); ++i) {
      denormalized[i] = output.data[i] * norm_scale_;
    }

    AVFrame* input_avframe = input_frame->frame();
    // Convert pixels to a temp frame for sws_scale
    absl::StatusOr<std::unique_ptr<Frame>> temp_frame = Frame::Create({});
    AVFrame* temp_avframe = (*temp_frame)->frame();
    temp_avframe->format = conv_format_;
    temp_avframe->width = input_avframe->width * out_scale;
    temp_avframe->height = input_avframe->height * out_scale;
    int size = av_image_fill_arrays(
        temp_avframe->data, temp_avframe->linesize, denormalized.data(),
        conv_format_, temp_avframe->width, temp_avframe->height, /*align=*/1);
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

 private:
  const AVPixelFormat conv_format_ = AV_PIX_FMT_GBR24P;
  const int norm_scale_ = 255;
};

}  // namespace

Upscaler::Upscaler(Model* model) : model_(model) {}

absl::StatusOr<std::unique_ptr<Frame>> Upscaler::Upscale(Frame* input_frame) {
  if (absl::Status status = Validate(input_frame); status != absl::OkStatus()) {
    return status;
  }

  Gbr24pConverter converter;

  absl::StatusOr<Model::ModelIo> input = converter.ToModelIo(input_frame);
  if (!input.ok()) {
    return input.status();
  }

  absl::StatusOr<Model::ModelIo> output = model_->RunModel(*input);
  if (!output.ok()) {
    return output.status();
  }

  return converter.FromModelIo(input_frame, *output, model_->getInfo().scale);
}

}  // namespace viduce::engine::upscale
