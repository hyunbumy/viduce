#include "engine/frame_reader.h"

#include <memory>
#include <string_view>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "engine/frame.h"
#include "engine/media_info.h"
#include "gtest/gtest.h"

namespace {

using ::viduce::engine::AudioInfo;
using ::viduce::engine::MediaInfo;
using ::viduce::engine::StreamInfo;
using ::viduce::engine::VideoInfo;

TEST(CreateTest, SuccessWithValidUrl) {
  std::string_view valid_url = "data/test.mp4";

  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(valid_url);

  EXPECT_TRUE(frame_reader.ok());
  EXPECT_NE(*frame_reader, nullptr);
}

TEST(CreateTest, FailWithInvalidUrl) {
  std::string_view invalid_url = "non_existent_url.mp4";

  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(invalid_url);

  EXPECT_FALSE(frame_reader.ok());
  EXPECT_EQ(frame_reader.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(MediaInfoTest, PopulateMediaInfo) {
  std::string_view valid_url = "data/test.mp4";
  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(valid_url);
  ABSL_ASSERT_OK(frame_reader);

  MediaInfo media_info = (*frame_reader)->media_info();

  EXPECT_EQ(media_info.streams.size(), 2);
  EXPECT_TRUE(std::any_of(media_info.streams.begin(), media_info.streams.end(),
                          [](const StreamInfo& s) {
                            return std::holds_alternative<VideoInfo>(
                                s.type_info);
                          }));
  EXPECT_TRUE(std::any_of(media_info.streams.begin(), media_info.streams.end(),
                          [](const StreamInfo& s) {
                            return std::holds_alternative<AudioInfo>(
                                s.type_info);
                          }));
}

TEST(ReadNextFrameTest, ReturnsAllValidFrames) {
  // Input video with 3 frames.
  std::string_view valid_url = "data/3frames_240x240.mp4";
  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(valid_url);
  ASSERT_TRUE(frame_reader.ok());

  int num_frames = 0;
  while (true) {
    absl::StatusOr<std::unique_ptr<viduce::engine::Frame>> frame =
        (*frame_reader)->ReadNextFrame();
    ABSL_ASSERT_OK(frame);
    if (*frame == nullptr) {
      break;
    }
    ++num_frames;
  }

  EXPECT_EQ(num_frames, 3);
}

}  // namespace
