#include "engine/frame_reader.h"

#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "engine/frame.h"
#include "gtest/gtest.h"

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
