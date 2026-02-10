#include "engine/frame_reader.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "engine/util.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}  // extern "C"

namespace {

// Return all codec contexts for the streams in the resource.
// The index of the codec corresponds to the stream index.
absl::StatusOr<std::vector<AVCodecContext*>> GetCodecs(
    AVFormatContext* format_ctx) {
  // Load stream info into format_ctx
  if (int res = avformat_find_stream_info(format_ctx, nullptr); res != 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "Failed to find stream info for video file %s with error: %s",
            format_ctx->url, AvErrToStr(res)));
  }

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
              "Failed to allocate codec context for url: %s, streamID: %s",
              format_ctx->url, i));
    }

    avcodec_parameters_to_context(codec_ctx, codecpar);
    avcodec_open2(codec_ctx, codec, nullptr);
    codecs.push_back(codec_ctx);
  }

  return codecs;
}

class Packet {
 public:
  Packet() {
    packet_ = av_packet_alloc();
  }

  ~Packet() { av_packet_unref(packet_); }

  AVPacket* get_packet() { return packet_; }

 private:
  // TODO: Figure out how to do static
  AVPacket* packet_;
};

}  // namespace

namespace viduce::engine {

absl::StatusOr<std::unique_ptr<FrameReader>> FrameReader::Create(
    std::string_view url) {
  AVFormatContext* format_ctx = avformat_alloc_context();
  int open_res = avformat_open_input(&format_ctx, url.data(), nullptr, nullptr);
  if (open_res != 0) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Failed to open input video file %s with error: %s",
                        url, AvErrToStr(open_res)));
  }

  absl::StatusOr<std::vector<AVCodecContext*>> codecs = GetCodecs(format_ctx);
  if (!codecs.ok()) {
    return codecs.status();
  }

  return std::unique_ptr<FrameReader>(new FrameReader(format_ctx, *codecs));
}

FrameReader::~FrameReader() {
  for (AVCodecContext* codec : codecs_) {
    avcodec_free_context(&codec);
  }
  avformat_free_context(format_ctx_);
}

absl::StatusOr<std::unique_ptr<Frame>> FrameReader::ReadNextFrame() {
  // Read encoded frames (aka packets)
  // av_read_frame will be responsible for reaching each _encoded_ frames which
  // will block until it's ready.
  // If input is being "streamed" in, then we must not block the thread:
  // https://stackoverflow.com/questions/23800615/ffmpeg-av-open-input-stream-in-latest-ffmpeg
  while (true) {
    Packet packet;
    AVPacket* av_packet = packet.get_packet();
    // av_read_frame will return packets from any of the streams
    int read_res = av_read_frame(format_ctx_, av_packet);
    if (read_res == AVERROR_EOF) {
      // Done processing
      return nullptr;
    }

    if (read_res != 0) {
      return absl::Status(absl::StatusCode::kInternal,
                          absl::StrFormat("Error reading frame with error: %s",
                                          AvErrToStr(read_res)));
    }

    AVCodecContext* codec = codecs_[av_packet->stream_index];
    if (int res = avcodec_send_packet(codec, av_packet); res != 0) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat("Error sending packet for decoding with "
                          "error: %s",
                          AvErrToStr(res)));
    }

    auto frame = std::make_unique<Frame>();
    AVFrame* av_frame = frame->get_frame();
    int decode_res = avcodec_receive_frame(codec, av_frame);
    if (decode_res == AVERROR(EAGAIN)) {
      // Need to send more packets before we can receive frames. Keep reading.
      continue;
    }

    if (decode_res != 0) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrFormat("Error receiving frame for decoding with "
                          "error: %s",
                          AvErrToStr(decode_res)));
    }

    return std::move(frame);
  }
}

}  // namespace viduce::engine
