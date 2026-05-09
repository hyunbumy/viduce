#include "engine/frame.h"

#include <memory>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame_reader.h"
#include "engine/media_info.h"
#include "gtest/gtest.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}  // extern "C"

namespace {

using ::viduce::engine::Frame;
using ::viduce::engine::StreamInfo;
using ::viduce::engine::VideoInfo;

TEST(CreateTest, Success) {
  StreamInfo stream_info{.stream_index = 1234,
                         .codec_id = AV_CODEC_ID_H264,
                         .type_info = VideoInfo{}};

  absl::StatusOr<std::unique_ptr<Frame>> frame = Frame::Create(stream_info);

  EXPECT_TRUE(frame.ok());
  EXPECT_NE((*frame)->frame(), nullptr);
  StreamInfo out_info = (*frame)->stream_info();
  EXPECT_EQ(out_info.stream_index, 1234);
  EXPECT_EQ(out_info.codec_id, AV_CODEC_ID_H264);
  EXPECT_TRUE(std::holds_alternative<VideoInfo>(out_info.type_info));
}

}  // namespace
