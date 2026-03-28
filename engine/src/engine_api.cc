#include "engine/engine_api.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/frame_reader.h"
#include "engine/upscale/model.h"
#include "engine/upscale/model_impl.h"
#include "engine/upscale/upscaler.h"
#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

}  // extern "C"

namespace {

using ::viduce::engine::Frame;
using ::viduce::engine::upscale::ModelImpl;
using ::viduce::engine::upscale::Upscaler;

absl::Status WriteToOutput(std::string_view output_dir, Frame* frame, int i) {
  AVFrame* avframe = frame->frame();
  int width = avframe->width;
  int height = avframe->height;

  SwsContext* sws_ctx = sws_getContext(
      width, height, (AVPixelFormat)avframe->format, width, height,
      AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

  AVFrame* rgb_frame = av_frame_alloc();
  rgb_frame->format = AV_PIX_FMT_RGB24;
  rgb_frame->width = width;
  rgb_frame->height = height;
  av_frame_get_buffer(rgb_frame, 0);
  sws_scale(sws_ctx, avframe->data, avframe->linesize, 0, height,
            rgb_frame->data, rgb_frame->linesize);

  // Write PPM File (P6 format: Binary RGB)
  std::filesystem::path fname = std::filesystem::path(output_dir) /
                                ("frame_" + std::to_string(i) + ".ppm");
  std::ofstream file(fname);
  if (!file) {
    return absl::InternalError("error opening file");
  }
  file << "P6\n" << width << " " << height << "\n255\n";
  for (int y = 0; y < height; y++) {
    file.write((char*)(rgb_frame->data[0] + y * rgb_frame->linesize[0]),
               width * 3);
  }
  file.close();

  // TODO: Cleanup properly
  av_frame_free(&rgb_frame);
  sws_freeContext(sws_ctx);

  return absl::OkStatus();
}

// TODO: Migrate these functionalities into a separate class for better
// testability
absl::Status EnhanceVideoInternal(std::string_view input_path,
                                 std::string_view output_dir) {
  std::string_view model_path = std::getenv("VIDUCE_UPSCALER_PATH");
  absl::StatusOr<ModelImpl> model = ModelImpl::Create(model_path);
  if (!model.ok()) {
    return model.status();
  }
  Upscaler upscaler(&(*model));

  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(input_path);
  if (!frame_reader.ok()) {
    return frame_reader.status();
  }

  int i = 0;
  while (true) {
    absl::StatusOr<std::unique_ptr<viduce::engine::Frame>> frame =
        (*frame_reader)->ReadNextFrame();
    if (!frame.ok()) {
      return frame.status();
    }

    if (*frame == nullptr) {
      // Done reading
      break;
    }

    viduce::engine::Frame::StreamInfo stream_info = (*frame)->stream_info();
    AVFrame* av_frame = (*frame)->frame();
    spdlog::info(
        "Decoded frame info: res ({}x{}), stream_index: {}, media_type: {}, "
        "codec_id: {}, pix_fmt: {}",
        av_frame->width, av_frame->height, stream_info.stream_index,
        av_get_media_type_string(stream_info.media_type),
        avcodec_get_name(stream_info.codec_id),
        av_get_pix_fmt_name(static_cast<AVPixelFormat>(av_frame->format)));

    absl::StatusOr<std::unique_ptr<viduce::engine::Frame>> upscaled =
        upscaler.Upscale(frame->get());
    if (!upscaled.ok()) {
      return upscaled.status();
    }

    absl::Status write_st = WriteToOutput(output_dir, upscaled->get(), i++);
    if (!write_st.ok()) {
      return write_st;
    }
  }

  return absl::OkStatus();
}

}  // namespace

int DecodeVideo(const char* input_path) {
  spdlog::error("deprecated");
  return absl::UnimplementedError("Deprecated").raw_code();
}

int EnhanceVideo(const char* input_path, const char* output_dir) {
  absl::Status status = EnhanceVideoInternal(input_path, output_dir);
  if (!status.ok()) {
    spdlog::error("EnhanceVideo failed: {}", status.message());
  }
  return status.raw_code();
}
