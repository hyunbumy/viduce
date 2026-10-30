#include "engine/interpolate/shm_buffer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <unistd.h>

namespace viduce::engine::interpolate {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;

std::string MakeUniqueShmName(std::string_view suffix) {
  return absl::StrFormat("/viduce_test_%d_%s", ::getpid(), suffix);
}

TEST(ShmBufferTest, CreateWriteAndOpenReadRoundTrip) {
  const std::string shm_name = MakeUniqueShmName("roundtrip");
  constexpr size_t kFrameBytes = 16;
  constexpr size_t kNumFrames = 3;
  constexpr size_t kTotalSize = kFrameBytes * kNumFrames;

  absl::StatusOr<std::unique_ptr<ShmBuffer>> creator =
      ShmBuffer::Create(shm_name, kTotalSize);
  ABSL_ASSERT_OK(creator);
  EXPECT_EQ((*creator)->size(), kTotalSize);
  EXPECT_EQ((*creator)->name(), shm_name);
  EXPECT_FALSE((*creator)->is_read_only());

  absl::StatusOr<std::unique_ptr<ShmBuffer>> reader =
      ShmBuffer::Open(shm_name, kTotalSize, /*read_only=*/true);
  ABSL_ASSERT_OK(reader);
  EXPECT_EQ((*reader)->size(), kTotalSize);
  EXPECT_TRUE((*reader)->is_read_only());

  for (size_t i = 0; i < kNumFrames; ++i) {
    std::vector<uint8_t> src(kFrameBytes, static_cast<uint8_t>(10 * (i + 1)));
    EXPECT_THAT((*creator)->WriteFrame(i, src.data(), kFrameBytes), IsOk());
  }

  for (size_t i = 0; i < kNumFrames; ++i) {
    std::vector<uint8_t> dst(kFrameBytes, 0);
    std::vector<uint8_t> expected(kFrameBytes,
                                  static_cast<uint8_t>(10 * (i + 1)));
    EXPECT_THAT((*reader)->ReadFrame(i, dst.data(), kFrameBytes), IsOk());
    EXPECT_THAT(dst, ElementsAreArray(expected));
  }
}

TEST(ShmBufferTest, CreatorDestructorUnlinksSegment) {
  const std::string shm_name = MakeUniqueShmName("unlink");
  {
    absl::StatusOr<std::unique_ptr<ShmBuffer>> creator =
        ShmBuffer::Create(shm_name, 64);
    ABSL_ASSERT_OK(creator);
  }

  // Segment should no longer exist after creator goes out of scope.
  absl::StatusOr<std::unique_ptr<ShmBuffer>> opened =
      ShmBuffer::Open(shm_name, 64);
  EXPECT_THAT(opened, StatusIs(absl::StatusCode::kNotFound));
}

TEST(ShmBufferTest, OpenWithoutLeadingSlashNormalizesName) {
  const std::string raw_name =
      absl::StrFormat("viduce_test_%d_noslash", ::getpid());
  absl::StatusOr<std::unique_ptr<ShmBuffer>> creator =
      ShmBuffer::Create(raw_name, 32);
  ABSL_ASSERT_OK(creator);
  EXPECT_EQ((*creator)->name(), "/" + raw_name);

  absl::StatusOr<std::unique_ptr<ShmBuffer>> opened =
      ShmBuffer::Open(raw_name, 32);
  ABSL_ASSERT_OK(opened);
  EXPECT_EQ((*opened)->name(), "/" + raw_name);
}

TEST(ShmBufferTest, CreateWithZeroSizeFails) {
  const std::string shm_name = MakeUniqueShmName("zero");
  EXPECT_THAT(ShmBuffer::Create(shm_name, 0),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be positive")));
}

TEST(ShmBufferTest, OpenTooSmallSegmentFails) {
  const std::string shm_name = MakeUniqueShmName("small");
  absl::StatusOr<std::unique_ptr<ShmBuffer>> creator =
      ShmBuffer::Create(shm_name, 32);
  ABSL_ASSERT_OK(creator);

  EXPECT_THAT(ShmBuffer::Open(shm_name, 64),
              StatusIs(absl::StatusCode::kFailedPrecondition,
                       HasSubstr("smaller than requested min_size")));
}

TEST(ShmBufferTest, WriteToReadOnlyBufferFails) {
  const std::string shm_name = MakeUniqueShmName("readonly");
  absl::StatusOr<std::unique_ptr<ShmBuffer>> creator =
      ShmBuffer::Create(shm_name, 32);
  ABSL_ASSERT_OK(creator);

  absl::StatusOr<std::unique_ptr<ShmBuffer>> ro =
      ShmBuffer::Open(shm_name, 32, /*read_only=*/true);
  ABSL_ASSERT_OK(ro);

  std::vector<uint8_t> data(16, 1);
  EXPECT_THAT((*ro)->WriteFrame(0, data.data(), data.size()),
              StatusIs(absl::StatusCode::kPermissionDenied));
}

TEST(ShmBufferTest, WriteAndReadBoundsChecks) {
  const std::string shm_name = MakeUniqueShmName("bounds");
  absl::StatusOr<std::unique_ptr<ShmBuffer>> buf =
      ShmBuffer::Create(shm_name, 32);
  ABSL_ASSERT_OK(buf);

  std::vector<uint8_t> frame(16, 42);
  // Null pointer checks.
  EXPECT_THAT((*buf)->WriteFrame(0, nullptr, 16),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT((*buf)->ReadFrame(0, nullptr, 16),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Zero frame_bytes checks.
  EXPECT_THAT((*buf)->WriteFrame(0, frame.data(), 0),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT((*buf)->ReadFrame(0, frame.data(), 0),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Out-of-bounds frame_index (index 2 * 16 = 32, 32 + 16 > 32).
  EXPECT_THAT((*buf)->WriteFrame(2, frame.data(), 16),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT((*buf)->ReadFrame(2, frame.data(), 16),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(ShmBufferTest, MoveSemanticsTransferOwnership) {
  const std::string shm_name = MakeUniqueShmName("move");
  absl::StatusOr<std::unique_ptr<ShmBuffer>> created =
      ShmBuffer::Create(shm_name, 32);
  ABSL_ASSERT_OK(created);

  ShmBuffer moved(std::move(**created));
  EXPECT_EQ(moved.size(), 32);
  EXPECT_NE(moved.data(), nullptr);
  EXPECT_EQ((*created)->data(), nullptr);
  EXPECT_EQ((*created)->size(), 0);
}

}  // namespace
}  // namespace viduce::engine::interpolate
