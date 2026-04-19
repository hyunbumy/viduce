#include "engine/frame_writer.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "engine/frame.h"
#include "engine/media_info.h"
#include "gtest/gtest.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}  // extern "C"

// libx265 has internal allocations (via `x265_malloc`) that persist past
// `avcodec_free_context` and trip LeakSanitizer. Suppress them so the test
// binary exits cleanly under ASan.
extern "C" const char* __lsan_default_suppressions() {
  return "leak:libx265\nleak:x265_malloc\n";
}

namespace {

using ::viduce::engine::Frame;
using ::viduce::engine::FrameWriter;
using ::viduce::engine::StreamInfo;
using ::viduce::engine::VideoInfo;

StreamInfo MakeVideoStreamInfo(int stream_index, int width, int height) {
  return StreamInfo{
      .stream_index = stream_index,
      .num_frames = 30,
      .codec_id = AV_CODEC_ID_H264,
      .time_base = AVRational{1, 30},
      .duration = 30,
      .type_info = VideoInfo{.dim = {.width = width, .height = height},
                             .pix_fmt = AV_PIX_FMT_YUV420P},
  };
}

TEST(CreateTest, Success) {
  std::string valid_url = "output.mp4";
  StreamInfo stream_info = MakeVideoStreamInfo(/*stream_index=*/0,
                                               /*width=*/64, /*height=*/64);

  absl::StatusOr<std::unique_ptr<FrameWriter>> frame_writer =
      FrameWriter::Create({.streams = {stream_info}}, {.url = valid_url});

  EXPECT_TRUE(frame_writer.ok());
  EXPECT_NE(*frame_writer, nullptr);
}

TEST(WriteTest, Success) {
  std::string valid_url = "output.mp4";
  StreamInfo stream_info = MakeVideoStreamInfo(/*stream_index=*/0,
                                               /*width=*/64, /*height=*/64);
  absl::StatusOr<std::unique_ptr<FrameWriter>> frame_writer =
      FrameWriter::Create({.streams = {stream_info}}, {.url = valid_url});
  ABSL_ASSERT_OK(frame_writer);

  absl::StatusOr<std::unique_ptr<Frame>> frame = Frame::Create(stream_info);
  ABSL_ASSERT_OK(frame);
  AVFrame* av_frame = (*frame)->frame();
  av_frame->format = AV_PIX_FMT_YUV420P;
  av_frame->width = 64;
  av_frame->height = 64;
  ASSERT_EQ(av_frame_get_buffer(av_frame, /*align=*/0), 0);
  av_frame->pts = 0;

  EXPECT_TRUE((*frame_writer)->Write(std::move(*frame)).ok());
  EXPECT_TRUE((*frame_writer)->Flush().ok());
}

}  // namespace
