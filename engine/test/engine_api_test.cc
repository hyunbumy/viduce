#include "engine/engine_api.h"

#include <string_view>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace {

TEST(DecodeVideoTest, SuccessfulDecode) {
  std::string_view valid_path = "data/test.mp4";

  int result = DecodeVideo(valid_path.data());

  EXPECT_EQ(static_cast<absl::StatusCode>(result), absl::StatusCode::kOk);
}

TEST(DecodeVideoTest, InvalidInputPathFails) {
  std::string_view invalid_path = "non_existent_video.mp4";

  int result = DecodeVideo(invalid_path.data());

  EXPECT_EQ(static_cast<absl::StatusCode>(result),
            absl::StatusCode::kInvalidArgument);
}

}  // namespace
