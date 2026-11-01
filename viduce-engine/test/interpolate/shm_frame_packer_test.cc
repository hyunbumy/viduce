#include "engine/interpolate/shm_frame_packer.h"
#include "engine/interpolate/shm_frame_unpacker.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "engine/frame.h"
#include "engine/interpolate/shm_buffer.h"
#include "engine/media_info.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <unistd.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}  // extern "C"

namespace viduce::engine::interpolate {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

constexpr int kRgbChannels = 3;

std::string MakeUniqueShmName(std::string_view suffix) {
  return absl::StrFormat("/viduce_packer_test_%d_%s", ::getpid(), suffix);
}

absl::StatusOr<std::unique_ptr<Frame>> CreateSyntheticRgbFrame(
    int width, int height, uint8_t base_val,
    StreamIndex stream_index = StreamIndex{0}) {
  absl::StatusOr<std::unique_ptr<Frame>> frame =
      Frame::Create(stream_index, AVMEDIA_TYPE_VIDEO);
  if (!frame.ok()) {
    return frame.status();
  }
  AVFrame* av_frame = (*frame)->frame();
  av_frame->width = width;
  av_frame->height = height;
  av_frame->format = AV_PIX_FMT_RGB24;
  if (av_frame_get_buffer(av_frame, /*align=*/32) < 0) {
    return absl::InternalError("Failed to allocate AVFrame buffer");
  }

  for (int y = 0; y < height; ++y) {
    uint8_t* row = av_frame->data[0] + y * av_frame->linesize[0];
    for (int x = 0; x < width; ++x) {
      row[x * 3 + 0] = static_cast<uint8_t>((base_val + x + y) & 0xFF);
      row[x * 3 + 1] = static_cast<uint8_t>((base_val + x * 2 + y) & 0xFF);
      row[x * 3 + 2] = static_cast<uint8_t>((base_val + x + y * 3) & 0xFF);
    }
  }
  return frame;
}

TEST(ShmFramePackerTest, PackAndUnpackRoundTripPreservesPixels) {
  constexpr int kWidth = 17;  // Odd width to exercise AVFrame row alignment padding.
  constexpr int kHeight = 12;
  constexpr size_t kNumFrames = 5;
  constexpr size_t kBytesPerFrame = kWidth * kHeight * kRgbChannels;

  std::vector<std::unique_ptr<Frame>> owned_frames;
  std::vector<Frame*> input_frames;
  for (size_t i = 0; i < kNumFrames; ++i) {
    absl::StatusOr<std::unique_ptr<Frame>> f = CreateSyntheticRgbFrame(
        kWidth, kHeight, static_cast<uint8_t>(i * 30), StreamIndex{2});
    ABSL_ASSERT_OK(f);
    input_frames.push_back(f->get());
    owned_frames.push_back(*std::move(f));
  }

  absl::StatusOr<std::unique_ptr<ShmBuffer>> shm =
      ShmBuffer::Create(MakeUniqueShmName("roundtrip"),
                        kNumFrames * kBytesPerFrame);
  ABSL_ASSERT_OK(shm);

  ShmFramePacker packer(kWidth, kHeight);
  EXPECT_THAT(packer.PackFrames(input_frames, shm->get()), IsOk());

  ShmFrameUnpacker unpacker(kWidth, kHeight, StreamIndex{2}, AV_PIX_FMT_RGB24);
  absl::StatusOr<std::vector<std::unique_ptr<Frame>>> unpacked =
      unpacker.UnpackFrames(**shm, kNumFrames);
  ABSL_ASSERT_OK(unpacked);
  ASSERT_EQ(unpacked->size(), kNumFrames);

  for (size_t i = 0; i < kNumFrames; ++i) {
    Frame* orig = input_frames[i];
    Frame* out = (*unpacked)[i].get();
    EXPECT_EQ(static_cast<int>(out->stream_index()), 2);
    EXPECT_EQ(out->media_type(), AVMEDIA_TYPE_VIDEO);

    AVFrame* orig_av = orig->frame();
    AVFrame* out_av = out->frame();
    ASSERT_EQ(out_av->width, kWidth);
    ASSERT_EQ(out_av->height, kHeight);
    ASSERT_EQ(out_av->format, AV_PIX_FMT_RGB24);
    EXPECT_EQ(out_av->pts, static_cast<int64_t>(i));

    for (int y = 0; y < kHeight; ++y) {
      const uint8_t* orig_row = orig_av->data[0] + y * orig_av->linesize[0];
      const uint8_t* out_row = out_av->data[0] + y * out_av->linesize[0];
      for (int b = 0; b < kWidth * kRgbChannels; ++b) {
        ASSERT_EQ(out_row[b], orig_row[b])
            << "Mismatch at frame " << i << " row " << y << " byte " << b;
      }
    }
  }
}

TEST(ShmFramePackerTest, PackYuv420pAndUnpackRoundTrip) {
  constexpr int kWidth = 16;
  constexpr int kHeight = 16;
  constexpr size_t kBytesPerFrame = kWidth * kHeight * kRgbChannels;

  absl::StatusOr<std::unique_ptr<Frame>> yuv_frame =
      Frame::Create(StreamIndex{0}, AVMEDIA_TYPE_VIDEO);
  ABSL_ASSERT_OK(yuv_frame);
  AVFrame* av_frame = (*yuv_frame)->frame();
  av_frame->width = kWidth;
  av_frame->height = kHeight;
  av_frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(av_frame, /*align=*/0), 0);

  // Fill neutral gray (Y=128, U=128, V=128).
  for (int y = 0; y < kHeight; ++y) {
    std::fill_n(av_frame->data[0] + y * av_frame->linesize[0], kWidth, 128);
  }
  for (int y = 0; y < kHeight / 2; ++y) {
    std::fill_n(av_frame->data[1] + y * av_frame->linesize[1], kWidth / 2, 128);
    std::fill_n(av_frame->data[2] + y * av_frame->linesize[2], kWidth / 2, 128);
  }

  absl::StatusOr<std::unique_ptr<ShmBuffer>> shm =
      ShmBuffer::Create(MakeUniqueShmName("yuv"), kBytesPerFrame);
  ABSL_ASSERT_OK(shm);

  ShmFramePacker packer(kWidth, kHeight);
  EXPECT_THAT(packer.PackFrames({yuv_frame->get()}, shm->get()), IsOk());

  ShmFrameUnpacker unpacker(kWidth, kHeight, StreamIndex{0},
                            AV_PIX_FMT_YUV420P);
  absl::StatusOr<std::vector<std::unique_ptr<Frame>>> unpacked =
      unpacker.UnpackFrames(**shm, 1);
  ABSL_ASSERT_OK(unpacked);
  ASSERT_EQ(unpacked->size(), 1);
  EXPECT_EQ((*unpacked)[0]->frame()->format, AV_PIX_FMT_YUV420P);
  EXPECT_NEAR((*unpacked)[0]->frame()->data[0][0], 128, 2);
}

TEST(ShmFramePackerTest, RejectsInvalidInputs) {
  constexpr int kWidth = 8;
  constexpr int kHeight = 8;
  constexpr size_t kBytesPerFrame = kWidth * kHeight * kRgbChannels;

  absl::StatusOr<std::unique_ptr<ShmBuffer>> shm =
      ShmBuffer::Create(MakeUniqueShmName("invalid"), kBytesPerFrame);
  ABSL_ASSERT_OK(shm);

  ShmFramePacker packer(kWidth, kHeight);
  // Null buffer.
  EXPECT_THAT(packer.PackFrames({}, nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
  // Empty frames.
  EXPECT_THAT(packer.PackFrames({}, shm->get()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  // Null frame pointer.
  EXPECT_THAT(packer.PackFrames({nullptr}, shm->get()),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Non-video frame.
  absl::StatusOr<std::unique_ptr<Frame>> audio_frame =
      Frame::Create(StreamIndex{0}, AVMEDIA_TYPE_AUDIO);
  ABSL_ASSERT_OK(audio_frame);
  EXPECT_THAT(packer.PackFrames({audio_frame->get()}, shm->get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("not a video frame")));

  // Mismatched dimensions.
  absl::StatusOr<std::unique_ptr<Frame>> wrong_dim =
      CreateSyntheticRgbFrame(4, 4, 0);
  ABSL_ASSERT_OK(wrong_dim);
  EXPECT_THAT(packer.PackFrames({wrong_dim->get()}, shm->get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("do not match packer")));

  // Buffer overflow (packing 2 frames into 1-frame SHM).
  absl::StatusOr<std::unique_ptr<Frame>> valid_frame =
      CreateSyntheticRgbFrame(kWidth, kHeight, 0);
  ABSL_ASSERT_OK(valid_frame);
  EXPECT_THAT(
      packer.PackFrames({valid_frame->get(), valid_frame->get()}, shm->get()),
      StatusIs(absl::StatusCode::kOutOfRange));

  // Unpacker bounds checks.
  ShmFrameUnpacker unpacker(kWidth, kHeight);
  EXPECT_THAT(unpacker.UnpackFrames(**shm, 0),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(unpacker.UnpackFrames(**shm, 2),
              StatusIs(absl::StatusCode::kOutOfRange));
}

}  // namespace
}  // namespace viduce::engine::interpolate
