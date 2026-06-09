#include "engine/frame_writer.h"

#include <memory>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/media_info.h"
#include "engine/util.h"
#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}  // extern "C"

namespace viduce::engine {

namespace {

using ::viduce::engine::util::AvErrToStr;

absl::StatusOr<AVFormatContext*> CreateFormatCtx(std::string_view url) {
  AVFormatContext* format_ctx;
  int avformat_res =
      avformat_alloc_output_context2(&format_ctx, nullptr, nullptr, url.data());
  if (avformat_res < 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "Failed to allocate output context for url: %s with error: %s", url,
            AvErrToStr(avformat_res)));
  }

  if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
    format_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  int open_res = avio_open2(&format_ctx->pb, url.data(), AVIO_FLAG_WRITE,
                            /*int_cb=*/nullptr, /*options=*/nullptr);
  if (open_res < 0) {
    return absl::Status(absl::StatusCode::kInternal,
                        absl::StrFormat("Failed to open url: %s with error: %s",
                                        url, AvErrToStr(open_res)));
  }

  return format_ctx;
}

// TODO: Figure out why there is a fps / duration mismatch
absl::StatusOr<FrameWriter::EncoderInfo> CreateVideoEncoderInfo(
    const StreamInfo& incoming_stream, AVFormatContext* format_ctx) {
  std::string codec_name = "libx265";
  const AVCodec* encoder = avcodec_find_encoder_by_name(codec_name.c_str());
  if (encoder == nullptr) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to find encoder with name: %s", codec_name));
  }

  AVCodecContext* encoder_ctx = avcodec_alloc_context3(encoder);
  if (encoder_ctx == nullptr) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to allocate codec context for encoder: %s",
                        codec_name));
  }
  absl::Cleanup encoder_ctx_cleanup(
      [&encoder_ctx] { avcodec_free_context(&encoder_ctx); });

  VideoInfo video_info = std::get<VideoInfo>(incoming_stream.type_info);
  encoder_ctx->height = video_info.dim.height;
  encoder_ctx->width = video_info.dim.width;
  encoder_ctx->pix_fmt = video_info.pix_fmt;

  // Control rate
  // encoder_ctx->bit_rate = 2 * 1000 * 1000;
  // encoder_ctx->rc_buffer_size = 4 * 1000 * 1000;
  // encoder_ctx->rc_max_rate = 2 * 1000 * 1000;
  // encoder_ctx->rc_min_rate = 2.5 * 1000 * 1000;

  // Set timebase of the encoder to be the same as the decoder to avoid needing
  // to convert timebase during encoding.
  encoder_ctx->time_base = incoming_stream.time_base;
  // TODO: This derives framerate from num_frames/duration, which divides by
  // zero (or produces nonsense) when the demuxer doesn't populate duration or
  // num_frames (live streams, fragmented MP4, etc.). Address once we support
  // target fps / target duration as planned in the Create() TODO above —
  // ideally by plumbing avg_frame_rate through StreamInfo.
  AVRational frame_rate;
  av_reduce(&frame_rate.num, &frame_rate.den,
            incoming_stream.num_frames * incoming_stream.time_base.den,
            incoming_stream.duration * incoming_stream.time_base.num,
            std::numeric_limits<int>::max());
  encoder_ctx->framerate = frame_rate;

  // Initialize encoder context
  int open_res = avcodec_open2(encoder_ctx, encoder, nullptr);
  if (open_res < 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to open encoder %s with error: %s", codec_name,
                        AvErrToStr(open_res)));
  }

  AVStream* stream = avformat_new_stream(format_ctx, nullptr);
  if (stream == nullptr) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to create new stream for stream index: %d",
                        static_cast<int>(incoming_stream.stream_index)));
  }
  // Set the same time_base for stream for now; we would need to update this
  // eventually once the framerate changes.
  stream->time_base = encoder_ctx->time_base;
  stream->avg_frame_rate = encoder_ctx->framerate;

  // Copy the encoder parameters to the stream
  int param_res =
      avcodec_parameters_from_context(stream->codecpar, encoder_ctx);
  if (param_res < 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "Failed to copy encoder parameters to stream for stream index: %d "
            "with error: %s",
            static_cast<int>(incoming_stream.stream_index),
            AvErrToStr(param_res)));
  }

  std::move(encoder_ctx_cleanup).Cancel();
  return FrameWriter::EncoderInfo{.encoding_stream = stream,
                                  .encoder_ctx = encoder_ctx};
}

absl::StatusOr<FrameWriter::EncoderInfo> CreateAudioEncoderInfo(
    const StreamInfo& stream_info, AVFormatContext* format_ctx) {
  AVStream* stream = avformat_new_stream(format_ctx, nullptr);
  if (stream == nullptr) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to create new stream for stream index: %d",
                        stream_info.stream_index));
  }

  return absl::UnimplementedError("Audio encoding is not implemented yet");
}

absl::StatusOr<FrameWriter::EncoderInfo> CreateEncoderInfo(
    const StreamInfo& stream_info, AVFormatContext* format_ctx) {
  if (std::holds_alternative<VideoInfo>(stream_info.type_info)) {
    return CreateVideoEncoderInfo(stream_info, format_ctx);
  }

  if (std::holds_alternative<AudioInfo>(stream_info.type_info)) {
    return CreateAudioEncoderInfo(stream_info, format_ctx);
  }

  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrFormat("Unsupported media type for stream "
                                      "index: %d",
                                      stream_info.stream_index));
}

}  // namespace

// TODO: We must also accept target fps and / or target duration.
// Probably target duration is better since that should not change (though we
// will also need to know the total number of frames). For now, assume same fps
absl::StatusOr<std::unique_ptr<FrameWriter>> FrameWriter::Create(
    const InputParams& input_params, const OutputParams& output_params) {
  absl::StatusOr<AVFormatContext*> format_ctx =
      CreateFormatCtx(output_params.url);
  if (!format_ctx.ok()) {
    return format_ctx.status();
  }
  absl::Cleanup format_ctx_cleanup([&format_ctx] {
    avio_closep(&(*format_ctx)->pb);
    avformat_free_context(*format_ctx);
  });

  std::unordered_map<StreamIndex, EncoderInfo> encoders;
  absl::Cleanup encoders_cleanup([&encoders] {
    for (auto& [_, info] : encoders) {
      avcodec_free_context(&info.encoder_ctx);
    }
  });
  for (const StreamInfo& stream_info : input_params.streams) {
    absl::StatusOr<EncoderInfo> encoder_info =
        CreateEncoderInfo(stream_info, *format_ctx);
    if (!encoder_info.ok()) {
      return encoder_info.status();
    }
    encoders[stream_info.stream_index] = *encoder_info;
  }

  if (int res = avformat_write_header(*format_ctx, nullptr); res < 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to write header for url: %s with error: %s",
                        output_params.url, AvErrToStr(res)));
  }

  AVPacket* packet = av_packet_alloc();
  if (packet == nullptr) {
    return absl::Status(absl::StatusCode::kInternal,
                        absl::StrFormat("Failed to allocate packet for url: %s",
                                        output_params.url));
  }

  std::move(format_ctx_cleanup).Cancel();
  std::move(encoders_cleanup).Cancel();
  return std::unique_ptr<FrameWriter>(
      new FrameWriter(*format_ctx, packet, encoders));
}

FrameWriter::FrameWriter(
    AVFormatContext* fmt_ctx, AVPacket* packet,
    const std::unordered_map<StreamIndex, EncoderInfo>& encoders)
    : format_ctx_(fmt_ctx), encoders_(encoders), packet_(packet) {}

FrameWriter::~FrameWriter() {
  av_packet_free(&packet_);
  for (auto& [_, encoder_info] : encoders_) {
    avcodec_free_context(&encoder_info.encoder_ctx);
  }
  avio_closep(&format_ctx_->pb);
  avformat_free_context(format_ctx_);
}

absl::Status FrameWriter::Write(std::unique_ptr<Frame> frame) {
  StreamIndex stream_index = frame->stream_index();
  if (encoders_.find(stream_index) == encoders_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("No encoder configured for stream index: %d",
                        static_cast<int>(stream_index)));
  }
  // Do the actual encoding
  return EncodeFrame(stream_index, frame->frame());
}

absl::Status FrameWriter::Flush() {
  for (const auto& [stream_index, encoder_info] : encoders_) {
    absl::Status encode_status = EncodeFrame(stream_index, /*frame=*/nullptr);
    if (!encode_status.ok()) {
      return encode_status;
    }
  }

  int write_res = av_write_trailer(format_ctx_);
  if (write_res < 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to write trailer with error: %s",
                        AvErrToStr(write_res)));
  }

  return absl::OkStatus();
}

absl::Status FrameWriter::EncodeFrame(StreamIndex stream_index,
                                      AVFrame* frame) {
  EncoderInfo encoder_info = encoders_.at(stream_index);
  AVCodecContext* encoder = encoder_info.encoder_ctx;
  // TODO: If a brand new frame is generated, we must ensure that pts is set
  // correctly for that frame.
  // Convert the pts from decoder's timebase to encoder's timebase;
  // alternatively though, I could just set the encoder's timebase to match the
  // decoder's timebase and avoid the need for conversion.
  int frame_result = avcodec_send_frame(encoder, frame);
  if (frame_result < 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Error sending frame for encoding with error: %s for "
                        "stream index: %d",
                        AvErrToStr(frame_result),
                        static_cast<int>(stream_index)));
  }

  while (true) {
    av_packet_unref(packet_);
    int packet_result = avcodec_receive_packet(encoder, packet_);
    if (packet_result == AVERROR_EOF || packet_result == AVERROR(EAGAIN)) {
      // Done with flushing encoder or need to send more frames in before
      // receiving more packets
      return absl::OkStatus();
    }

    if (packet_result < 0) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat(
              "Error receiving packet for encoding with error: %s for "
              "stream index: %d",
              AvErrToStr(packet_result), static_cast<int>(stream_index)));
    }

    spdlog::debug("Encoded packet for stream index: {} with size: {}",
                  static_cast<int>(stream_index), packet_->size);

    AVStream* encoding_stream = encoder_info.encoding_stream;

    // TODO: Figure out what data need to be populated
    packet_->stream_index = encoding_stream->index;

    // Re-adjust timebase from encoder's timebase to stream's timebase
    av_packet_rescale_ts(packet_, encoder->time_base,
                         encoding_stream->time_base);
    int res = av_interleaved_write_frame(format_ctx_, packet_);
    if (res < 0) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat(
              "Error writing packet with error: %s for stream index: "
              "%d",
              AvErrToStr(res), static_cast<int>(stream_index)));
    }
  }
}

}  // namespace viduce::engine
