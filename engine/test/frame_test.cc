#include "engine/frame.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "engine/media_info.h"
#include "gtest/gtest.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
}  // extern "C"

namespace {

using ::viduce::engine::Frame;
using ::viduce::engine::StreamIndex;

TEST(CreateTest, Success) {
  absl::StatusOr<std::unique_ptr<Frame>> frame =
      Frame::Create(StreamIndex{1234}, AVMEDIA_TYPE_VIDEO);

  EXPECT_TRUE(frame.ok());
  EXPECT_NE((*frame)->frame(), nullptr);
  EXPECT_EQ(static_cast<int>((*frame)->stream_index()), 1234);
  EXPECT_EQ((*frame)->media_type(), AVMEDIA_TYPE_VIDEO);
}

TEST(CloneTest, CopiesStreamIndexAndMediaType) {
  absl::StatusOr<std::unique_ptr<Frame>> frame =
      Frame::Create(StreamIndex{1234}, AVMEDIA_TYPE_VIDEO);
  ABSL_ASSERT_OK(frame);

  absl::StatusOr<std::unique_ptr<Frame>> cloned = Frame::Clone(frame->get());

  EXPECT_TRUE(cloned.ok());
  EXPECT_EQ(static_cast<int>((*cloned)->stream_index()), 1234);
  EXPECT_EQ((*cloned)->media_type(), AVMEDIA_TYPE_VIDEO);
}

TEST(CloneTest, CopiesAvFramePropsButNotGeometry) {
  // Lock in the documented contract: av_frame_copy_props carries pts/duration
  // and color metadata, but width/height/format/linesize are NOT carried.
  absl::StatusOr<std::unique_ptr<Frame>> frame =
      Frame::Create(StreamIndex{0}, AVMEDIA_TYPE_VIDEO);
  ABSL_ASSERT_OK(frame);
  AVFrame* avframe = (*frame)->frame();
  avframe->pts = 42;
  avframe->pkt_dts = 1111;
  avframe->pkt_duration = 2222;
  avframe->width = 64;
  avframe->height = 48;
  avframe->format = AV_PIX_FMT_YUV420P;

  absl::StatusOr<std::unique_ptr<Frame>> cloned = Frame::Clone(frame->get());
  ABSL_ASSERT_OK(cloned);
  AVFrame* cloned_avframe = (*cloned)->frame();

  EXPECT_EQ(cloned_avframe->pts, 42);
  EXPECT_EQ(cloned_avframe->pkt_dts, 1111);
  EXPECT_EQ(cloned_avframe->pkt_duration, 2222);
  EXPECT_EQ(cloned_avframe->width, 0);
  EXPECT_EQ(cloned_avframe->height, 0);
  EXPECT_EQ(cloned_avframe->format, AV_PIX_FMT_NONE);
}

}  // namespace
