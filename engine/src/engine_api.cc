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
#include "engine/frame_writer.h"
#include "engine/media_info.h"
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

namespace viduce::engine {

namespace {

using ::viduce::engine::upscale::ModelImpl;
using ::viduce::engine::upscale::Upscaler;

// TODO: Migrate these functionalities into a separate class for better
// testability
// TODO: Always just copy the audio as-is without re-encoding
absl::Status EnhanceVideoInternal(std::string_view input_path,
                                  std::string_view output_dir) {
  std::string_view model_path = std::getenv("VIDUCE_UPSCALER_PATH");
  absl::StatusOr<ModelImpl> model = ModelImpl::Create(model_path);
  if (!model.ok()) {
    return model.status();
  }
  Upscaler upscaler(&(*model));

  // TODO: Add a separate stage for reading the input file for info gathering
  // such as duration, total num of streams, frames, etc.
  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(input_path);
  if (!frame_reader.ok()) {
    return frame_reader.status();
  }

  std::vector<StreamInfo> input_streams =
      (*frame_reader)->media_info().streams;
  for (StreamInfo& stream_info : input_streams) {
    if (std::holds_alternative<VideoInfo>(stream_info.type_info)) {
      // For now we just assume the output video stream has the same metadata
      // except for the dimensions which are 4x upscaled. We will need to
      // update this eventually once we support more dynamic metadata changes.
      VideoInfo video_info = std::get<VideoInfo>(stream_info.type_info);
      video_info.dim.width *= 4;
      video_info.dim.height *= 4;
      stream_info.type_info = video_info;
    }
  }
  FrameWriter::InputParams input_params{.streams = input_streams};
  std::string output_url = std::filesystem::path(output_dir) / "output.mp4";
  FrameWriter::OutputParams output_params{.url = output_url};
  absl::StatusOr<std::unique_ptr<FrameWriter>> frame_writer =
      FrameWriter::Create(input_params, output_params);
  if (!frame_writer.ok()) {
    return frame_writer.status();
  }

  int i = 0;
  while (true) {
    // TODO: Provide concurrency per stage by using multithreading + queues.
    absl::StatusOr<std::unique_ptr<viduce::engine::Frame>> frame =
        (*frame_reader)->ReadNextFrame();
    if (!frame.ok()) {
      return frame.status();
    }

    if (*frame == nullptr) {
      // Done reading
      break;
    }

    viduce::engine::StreamInfo stream_info = (*frame)->stream_info();
    AVFrame* av_frame = (*frame)->frame();
    spdlog::info(
        "Decoded frame info: res ({}x{}), stream_index: {}, codec_id: {}, "
        "pix_fmt: {}",
        av_frame->width, av_frame->height, stream_info.stream_index,
        avcodec_get_name(stream_info.codec_id),
        av_get_pix_fmt_name(static_cast<AVPixelFormat>(av_frame->format)));

    absl::StatusOr<std::unique_ptr<viduce::engine::Frame>> upscaled =
        upscaler.Upscale(frame->get());
    if (!upscaled.ok()) {
      return upscaled.status();
    }

    absl::Status write_status = (*frame_writer)->Write(std::move(*upscaled));
    if (!write_status.ok()) {
      return write_status;
    }
  }

  absl::Status flush_status = (*frame_writer)->Flush();
  if (!flush_status.ok()) {
    return flush_status;
  }

  return absl::OkStatus();
}

}  // namespace

}  // namespace viduce::engine

int DecodeVideo(const char* input_path) {
  spdlog::error("deprecated");
  return absl::UnimplementedError("Deprecated").raw_code();
}

int EnhanceVideo(const char* input_path, const char* output_dir) {
  absl::Status status =
      viduce::engine::EnhanceVideoInternal(input_path, output_dir);
  if (!status.ok()) {
    spdlog::error("EnhanceVideo failed: {}", status.message());
  }
  return status.raw_code();
}
