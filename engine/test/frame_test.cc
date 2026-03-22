#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame_reader.h"
#include "gtest/gtest.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}  // extern "C"

namespace {

using ::viduce::engine::Frame;

TEST(CreateTest, Success) {
  Frame::StreamInfo stream_info{.stream_index = 1234,
                                .media_type = AVMEDIA_TYPE_VIDEO,
                                .codec_id = AV_CODEC_ID_H264};

  absl::StatusOr<std::unique_ptr<Frame>> frame = Frame::Create(stream_info);

  EXPECT_TRUE(frame.ok());
  EXPECT_NE((*frame)->frame(), nullptr);
  Frame::StreamInfo out_info = (*frame)->stream_info();
  EXPECT_EQ(out_info.stream_index, 1234);
  EXPECT_EQ(out_info.media_type, AVMEDIA_TYPE_VIDEO);
  EXPECT_EQ(out_info.codec_id, AV_CODEC_ID_H264);
}

}  // namespace
