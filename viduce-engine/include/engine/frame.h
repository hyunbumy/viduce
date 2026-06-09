#pragma once

#include <memory>

#include "absl/status/statusor.h"
#include "engine/media_info.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}  // extern "C"

namespace viduce::engine {

class Frame {
 public:
  // Creates a new Frame tagged with the given stream index and media type.
  // The underlying AVFrame will NOT be populated with any data.
  static absl::StatusOr<std::unique_ptr<Frame>> Create(
      StreamIndex stream_index, AVMediaType media_type);

  // Clones a Frame into a new Frame with the same stream index and media type.
  // The new Frame's AVFrame is allocated empty and then has metadata copied
  // from the source via av_frame_copy_props — that covers timing/color props
  // (pts, duration, color_range, color_primaries, etc.) but does NOT carry
  // over geometry/format fields (width, height, format, linesize) or any
  // pixel buffers. Callers that need those must set them on the clone
  // explicitly.
  static absl::StatusOr<std::unique_ptr<Frame>> Clone(Frame* frame);

  ~Frame();

  AVFrame* frame() { return frame_; }

  // Index of the stream that this frame belongs to. Used by FrameWriter to
  // dispatch the frame to the right encoder. Stream-level metadata (codec,
  // timebase, dimensions of the source, etc.) lives on MediaInfo, not on the
  // frame, so it never goes stale when the frame is transformed.
  StreamIndex stream_index() const { return stream_index_; }

  // Media type of the source stream (video / audio / subtitle / etc.). This
  // describes the *origin* of the frame and does not change when the frame is
  // transformed.
  AVMediaType media_type() const { return media_type_; }

 private:
  explicit Frame(StreamIndex stream_index, AVMediaType media_type,
                 AVFrame* frame)
      : frame_(frame), stream_index_(stream_index), media_type_(media_type) {}

  AVFrame* frame_;
  StreamIndex stream_index_;
  AVMediaType media_type_;
};

}  // namespace viduce::engine
