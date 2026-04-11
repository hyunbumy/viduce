#include "engine/frame_writer.h"

#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/util.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}  // extern "C"

namespace viduce::engine {

namespace {

using ::viduce::engine::util::AvErrToStr;

absl::StatusOr<AVFormatContext*> CreateFormatCtx(std::string_view url) {
  AVFormatContext* format_ctx;
  int avformat_res =
      avformat_alloc_output_context2(&format_ctx, nullptr, nullptr, url.data());
  if (avformat_res != 0) {
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
  if (open_res != 0) {
    return absl::Status(absl::StatusCode::kInternal,
                        absl::StrFormat("Failed to open url: %s with error: %s",
                                        url, AvErrToStr(open_res)));
  }

  return format_ctx;
}

absl::StatusOr<FrameWriter::EncoderInfo> CreateVideoEncoderInfo(
    const Frame::StreamInfo& stream_info, AVFormatContext* format_ctx) {
  AVStream* stream = avformat_new_stream(format_ctx, nullptr);
  if (stream == nullptr) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to create new stream for stream index: %d",
                        stream_info.stream_index));
  }

  return absl::UnimplementedError("Video encoding is not implemented yet");
}

absl::StatusOr<FrameWriter::EncoderInfo> CreateAudioEncoderInfo(
    const Frame::StreamInfo& stream_info, AVFormatContext* format_ctx) {
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
    const Frame::StreamInfo& stream_info, AVFormatContext* format_ctx) {
  if (stream_info.media_type == AVMEDIA_TYPE_VIDEO) {
    return CreateVideoEncoderInfo(stream_info, format_ctx);
  }

  if (stream_info.media_type == AVMEDIA_TYPE_AUDIO) {
    return CreateAudioEncoderInfo(stream_info, format_ctx);
  }

  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrFormat("Unsupported media type: %d for stream "
                      "index: %d",
                      stream_info.media_type, stream_info.stream_index));
}

}  // namespace

// TODO: We must also accept target fps and / or target duration.
// Probably target duration is better since that should not change (though we
// will also need to know the total number of frames)
absl::StatusOr<std::unique_ptr<FrameWriter>> FrameWriter::Create(
    std::string_view output_url) {
  absl::StatusOr<AVFormatContext*> format_ctx = CreateFormatCtx(output_url);
  if (!format_ctx.ok()) {
    return format_ctx.status();
  }

  return absl::UnimplementedError("FrameWriter is not implemented yet");
}

FrameWriter::~FrameWriter() {
  av_packet_free(&packet_);
  avformat_close_input(&format_ctx_);
}

absl::Status FrameWriter::Write(std::unique_ptr<Frame> frame) {
  Frame::StreamInfo stream_info = frame->stream_info();
  if (encoders_.find(stream_info.stream_index) == encoders_.end()) {
    // TODO: Create a new encoder for the stream if it doesn't exist yet.
    encoders_[stream_info.stream_index] =
        *CreateEncoderInfo(stream_info, format_ctx_);
  }

  // Do the actual encoding
  EncoderInfo encoder_info = encoders_.at(stream_info.stream_index);
  AVCodecContext* encoder = encoder_info.encoder_ctx;
  // TODO: If a brand new frame is generated, we must ensure that pts is set
  // correctly for that frame.
  int frame_result = avcodec_send_frame(encoder, frame->frame());
  if (frame_result != 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Error sending frame for encoding with error: %s for "
                        "stream index: %d",
                        AvErrToStr(frame_result), stream_info.stream_index));
  }

  av_packet_unref(packet_);
  int packet_result = avcodec_receive_packet(encoder, packet_);
  if (packet_result == AVERROR(EAGAIN) || packet_result == AVERROR_EOF) {
    return absl::OkStatus();
  }
  if (packet_result != 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "Error receiving packet for encoding with error: %s for "
            "stream index: %d",
            AvErrToStr(packet_result), stream_info.stream_index));
  }

  // TODO: Figure out what data need to be populated
  packet_->stream_index = encoder_info.encoding_stream->index;
  packet_->duration = frame->frame()->pkt_duration;

  // Re-adjust timebase from encoder's timebase to stream's timebase
  av_packet_rescale_ts(
      packet_, encoder->time_base,
      format_ctx_->streams[encoder_info.encoding_stream->index]->time_base);
  av_interleaved_write_frame(format_ctx_, packet_);
}

}  // namespace viduce::engine
