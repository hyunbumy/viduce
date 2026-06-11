#include "engine/frame_reader.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
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

absl::StatusOr<AVFormatContext*> OpenInput(std::string_view url) {
  AVFormatContext* format_ctx = avformat_alloc_context();
  int open_res = avformat_open_input(&format_ctx, url.data(), nullptr, nullptr);
  if (open_res != 0) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Failed to open input video file %s with error: %s",
                        url, util::AvErrToStr(open_res)));
  }
  // Load stream info into format_ctx
  if (int res = avformat_find_stream_info(format_ctx, nullptr); res != 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "Failed to find stream info for video file %s with error: %s",
            format_ctx->url, util::AvErrToStr(res)));
  }

  return format_ctx;
}

// Return all codec contexts for the streams in the resource.
// The index of the codec corresponds to the stream index.
absl::StatusOr<std::vector<AVCodecContext*>> GetCodecs(
    AVFormatContext* format_ctx) {
  // Build codec for each stream
  std::vector<AVCodecContext*> codecs;
  for (int i = 0; i < format_ctx->nb_streams; ++i) {
    AVCodecParameters* codecpar = format_ctx->streams[i]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (codec == nullptr) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat("Failed to find the codec for the given stream info "
                          "for url %s (codecID: %d)",
                          format_ctx->url, codecpar->codec_id));
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == nullptr) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat(
              "Failed to allocate codec context for url: %s, streamID: %d",
              format_ctx->url, i));
    }

    avcodec_parameters_to_context(codec_ctx, codecpar);
    // Propagate the timebase from stream to codec context
    codec_ctx->pkt_timebase = format_ctx->streams[i]->time_base;
    if (int open_res = avcodec_open2(codec_ctx, codec, nullptr); open_res < 0) {
      avcodec_free_context(&codec_ctx);
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat("Failed to open codec for url: %s, streamID: %d "
                          "with error: %s",
                          format_ctx->url, i, AvErrToStr(open_res)));
    }
    codecs.push_back(codec_ctx);
  }

  return codecs;
}

MediaInfo CreateMediaInfo(const AVFormatContext* format_ctx) {
  MediaInfo media_info;
  for (int i = 0; i < format_ctx->nb_streams; ++i) {
    AVStream* stream = format_ctx->streams[i];
    StreamInfo stream_info;
    stream_info.stream_index = StreamIndex{stream->index};
    stream_info.num_frames = stream->nb_frames;
    stream_info.time_base = stream->time_base;
    stream_info.duration = stream->duration;
    stream_info.codec_id = stream->codecpar->codec_id;

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      stream_info.type_info = VideoInfo{
          .dim = VideoInfo::Dimension{.width = stream->codecpar->width,
                                      .height = stream->codecpar->height},
          .pix_fmt = static_cast<AVPixelFormat>(stream->codecpar->format)};
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      stream_info.type_info = AudioInfo{};
    }

    media_info.streams.push_back(stream_info);
  }

  return media_info;
}

void CleanupFormatCtx(AVFormatContext* format_ctx) {
  avformat_close_input(&format_ctx);
  avformat_free_context(format_ctx);
}

void CleanupCodecCtxs(std::vector<AVCodecContext*> codec_ctxs) {
  for (AVCodecContext* codec_ctx : codec_ctxs) {
    avcodec_free_context(&codec_ctx);
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<FrameReader>> FrameReader::Create(
    std::string_view url) {
  absl::StatusOr<AVFormatContext*> format_ctx = OpenInput(url);
  if (!format_ctx.ok()) {
    return format_ctx.status();
  }
  absl::Cleanup avformat_cleanup(
      [&format_ctx] { CleanupFormatCtx(*format_ctx); });

  absl::StatusOr<std::vector<AVCodecContext*>> codecs = GetCodecs(*format_ctx);
  if (!codecs.ok()) {
    return codecs.status();
  }
  absl::Cleanup avcodec_cleanup([&codecs] { CleanupCodecCtxs(*codecs); });

  AVPacket* packet = av_packet_alloc();
  if (packet == nullptr) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat("Failed to allocate packet for url: %s", url));
  }

  std::move(avformat_cleanup).Cancel();
  std::move(avcodec_cleanup).Cancel();
  return std::unique_ptr<FrameReader>(new FrameReader(
      *format_ctx, packet, std::move(*codecs), CreateMediaInfo(*format_ctx)));
}

FrameReader::~FrameReader() {
  av_packet_free(&packet_);
  CleanupCodecCtxs(codecs_);
  CleanupFormatCtx(format_ctx_);
}

absl::StatusOr<std::unique_ptr<Frame>> FrameReader::ReadNextFrame() {
  // Read encoded frames (aka packets)
  // av_read_frame will be responsible for reaching each _encoded_ frames which
  // will block until it's ready.
  // If input is being "streamed" in, then we must not block the thread:
  // https://stackoverflow.com/questions/23800615/ffmpeg-av-open-input-stream-in-latest-ffmpeg
  while (true) {
    av_packet_unref(packet_);

    // av_read_frame will return packets from any of the streams
    int read_res = av_read_frame(format_ctx_, packet_);
    // Keep reading even though EOF for flushing decoder.
    if (read_res != 0 && read_res != AVERROR_EOF) {
      return absl::Status(absl::StatusCode::kInternal,
                          absl::StrFormat("Error reading frame with error: %s",
                                          util::AvErrToStr(read_res)));
    }

    if (packet_->stream_index < 0 ||
        static_cast<size_t>(packet_->stream_index) >= codecs_.size()) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat(
              "Packet has out-of-range stream_index: %d (have %d streams)",
              packet_->stream_index, codecs_.size()));
    }

    AVCodecContext* codec = codecs_[packet_->stream_index];
    int packet_res = avcodec_send_packet(codec, packet_);
    if (packet_res != 0 && packet_res != AVERROR_EOF) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat("Error sending packet for decoding with "
                          "error: %s",
                          util::AvErrToStr(packet_res)));
    }

    absl::StatusOr<std::unique_ptr<Frame>> frame =
        Frame::Create(StreamIndex{packet_->stream_index},
                      format_ctx_->streams[packet_->stream_index]
                          ->codecpar->codec_type);
    if (!frame.ok()) {
      return frame;
    }
    AVFrame* av_frame = (*frame)->frame();
    int decode_res = avcodec_receive_frame(codec, av_frame);
    if (decode_res == AVERROR(EAGAIN)) {
      // Need to send more packets before we can receive frames. Keep reading.
      continue;
    }

    if (decode_res == AVERROR_EOF) {
      // Done with reading all frames.
      return nullptr;
    }

    if (decode_res != 0) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat("Error receiving frame for decoding with "
                          "error: %s",
                          util::AvErrToStr(decode_res)));
    }

    return std::move(frame);
  }
}

}  // namespace viduce::engine
