#include "engine/upscale/upscaler.h"

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "engine/frame.h"
#include "gtest/gtest.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}  // extern "C"

namespace {

using ::viduce::engine::Frame;
using ::viduce::engine::upscale::Upscaler;

TEST(CreateTest, Successful) {}

TEST(UpscaleTest, CreatesNewFrame) {
  absl::StatusOr<std::unique_ptr<Frame>> input =
      Frame::Create({.media_type = AVMEDIA_TYPE_VIDEO});
  ABSL_ASSERT_OK(input);
  AVFrame* frame = (*input)->frame();
  frame->width = 2;
  frame->height = 1;
  frame->format = AV_PIX_FMT_RGB24;
  std::vector<uint8_t> data(6, 255);
  av_image_fill_arrays(frame->data, frame->linesize, data.data(),
                       AV_PIX_FMT_RGB24, frame->width, frame->height,
                       /*align=*/1);

  absl::StatusOr<Upscaler> upscaler = Upscaler::Create("bin/realesrgan.tflite");
  ABSL_ASSERT_OK(upscaler);
  absl::StatusOr<std::unique_ptr<Frame>> new_frame =
      upscaler->Upscale(input->get());

  EXPECT_THAT(new_frame, absl_testing::IsOk());
  AVFrame* frame = (*new_frame)->frame();
  EXPECT_EQ(frame->width, 8);
  EXPECT_EQ(frame->height, 4);
  EXPECT_EQ(frame->format, (int)AV_PIX_FMT_RGB24);
}

}  // namespace
