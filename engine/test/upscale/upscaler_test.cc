#include "engine/upscale/upscaler.h"

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "engine/frame.h"
#include "engine/upscale/model.h"
#include "gtest/gtest.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}  // extern "C"

namespace {

using ::viduce::engine::Frame;
using ::viduce::engine::upscale::Model;
using ::viduce::engine::upscale::Upscaler;

// Number of channels in RGB24
constexpr int kRgbChannel = 3;

class MockModel : public Model {
 public:
  Info getInfo() override { return {.scale = 2}; }

  absl::StatusOr<Model::ModelIo> RunModel(
      const Model::ModelIo& input) override {
    if (should_fail_) {
      return absl::InternalError("Mock model failure");
    }
    last_input_ = input;
    std::vector<float> output;
    output.reserve(input.data.size() * 4);
    for (int i = 0; i < 4; ++i) {
      output.insert(output.end(), input.data.begin(), input.data.end());
    }
    return Model::ModelIo{.data = std::move(output)};
  }

  Model::ModelIo last_input_;
  bool should_fail_ = false;
};

TEST(UpscaleTest, CreatesNewFrame) {
  absl::StatusOr<std::unique_ptr<Frame>> input =
      Frame::Create({.media_type = AVMEDIA_TYPE_VIDEO});
  ABSL_ASSERT_OK(input);
  AVFrame* frame = (*input)->frame();
  // NOTE: We actually must use at least 32-byte input since sws_scale will
  // automatically pad the output during conversion for SIMD alignment (usually
  // 32-byte alignment according to
  // https://stackoverflow.com/questions/31253485/how-do-you-resize-an-avframe/31270501#31270501)
  frame->width = 32;
  frame->height = 1;
  frame->format = AV_PIX_FMT_RGB24;
  std::vector<uint8_t> data(frame->width * frame->height * kRgbChannel, 123);
  av_image_fill_arrays(frame->data, frame->linesize, data.data(),
                       AV_PIX_FMT_RGB24, frame->width, frame->height,
                       /*align=*/1);

  MockModel model;
  Upscaler upscaler(&model);
  absl::StatusOr<std::unique_ptr<Frame>> res = upscaler.Upscale(input->get());

  EXPECT_THAT(res, absl_testing::IsOk());
  AVFrame* new_frame = (*res)->frame();
  EXPECT_EQ(new_frame->width, 64);
  EXPECT_EQ(new_frame->height, 2);
  EXPECT_EQ(new_frame->format, (int)AV_PIX_FMT_RGB24);

  // linesize is the width of the video  * # of channels in bytes.
  EXPECT_EQ(
      new_frame->linesize[0],
      av_image_get_linesize(AV_PIX_FMT_RGB24, new_frame->width, /*plane=*/0));
  EXPECT_EQ(new_frame->linesize[0], new_frame->width * kRgbChannel);
  int expected_output_size = (32 * 2) * (1 * 2) * kRgbChannel;
  EXPECT_EQ(av_image_get_buffer_size(AV_PIX_FMT_RGB24, new_frame->width,
                                     new_frame->height, /*stride=*/32),
            expected_output_size);
  for (int i = 0; i < expected_output_size; ++i) {
    EXPECT_EQ(new_frame->data[0][i], 123);
  }
}

TEST(UpscaleTest, ModelError) {
  absl::StatusOr<std::unique_ptr<Frame>> input =
      Frame::Create({.media_type = AVMEDIA_TYPE_VIDEO});
  ABSL_ASSERT_OK(input);
  AVFrame* frame = (*input)->frame();
  frame->width = 2;
  frame->height = 1;
  frame->format = AV_PIX_FMT_RGB24;
  std::vector<uint8_t> data(6, 123);
  av_image_fill_arrays(frame->data, frame->linesize, data.data(),
                       AV_PIX_FMT_RGB24, frame->width, frame->height,
                       /*align=*/1);

  MockModel model;
  model.should_fail_ = true;
  Upscaler upscaler(&model);
  absl::StatusOr<std::unique_ptr<Frame>> res = upscaler.Upscale(input->get());

  EXPECT_THAT(res,
              absl_testing::StatusIs(absl::StatusCode::kInternal,
                                     testing::HasSubstr("Mock model failure")));
}

}  // namespace
