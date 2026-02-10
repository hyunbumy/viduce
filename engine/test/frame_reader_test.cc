#include "engine/frame_reader.h"

#include <memory>
#include <string_view>

#include "absl/status/status.h"
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

TEST(ReadNextFrameTest, ReturnsValidFrame) {
  std::string_view valid_url = "data/test.mp4";
  absl::StatusOr<std::unique_ptr<viduce::engine::FrameReader>> frame_reader =
      viduce::engine::FrameReader::Create(valid_url);
  ASSERT_TRUE(frame_reader.ok());

  absl::StatusOr<std::unique_ptr<viduce::engine::Frame>> frame =
      (*frame_reader)->ReadNextFrame();

  EXPECT_TRUE(frame.ok());
  EXPECT_NE(*frame, nullptr);
}
