#include "engine/interpolate/ldf_vfi_client.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "engine/frame.h"
#include "engine/interpolate/interpolator.h"
#include "engine/interpolate/shm_buffer.h"
#include "engine/media_info.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "ldfvfi/v1/service.grpc.pb.h"
#include "ldfvfi/v1/service.pb.h"
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

class FakeLdfVfiService final : public viduce::ldfvfi::LdfVfiService::Service {
 public:
  grpc::Status CreateSession(
      grpc::ServerContext* context,
      const viduce::ldfvfi::CreateSessionRequest* request,
      viduce::ldfvfi::CreateSessionResponse* response) override {
    if (fail_create_) {
      return grpc::Status(grpc::StatusCode::INTERNAL, "Mock create failure");
    }
    last_create_req_ = *request;
    session_id_ = absl::StrFormat("test_%d_%d", ::getpid(), ++session_counter_);

    const size_t frame_bytes =
        static_cast<size_t>(request->media_metadata().width()) *
        static_cast<size_t>(request->media_metadata().height()) * kRgbChannels;
    const size_t temporal_sf =
        request->params().temporal_scale_factor() > 0
            ? static_cast<size_t>(request->params().temporal_scale_factor())
            : 4;

    auto in_shm = ShmBuffer::Create(absl::StrCat("/viduce_", session_id_, "_in"),
                                    frame_bytes * 10);
    if (!in_shm.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(in_shm.status().message()));
    }
    auto out_shm =
        ShmBuffer::Create(absl::StrCat("/viduce_", session_id_, "_out"),
                          frame_bytes * 10 * temporal_sf);
    if (!out_shm.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(out_shm.status().message()));
    }

    in_shm_ = std::move(*in_shm);
    out_shm_ = std::move(*out_shm);
    frame_bytes_ = frame_bytes;
    temporal_sf_ = temporal_sf;
    response->set_session_id(session_id_);
    return grpc::Status::OK;
  }

  grpc::Status InterpolateChunk(
      grpc::ServerContext* context,
      const viduce::ldfvfi::InterpolateChunkRequest* request,
      viduce::ldfvfi::InterpolateChunkResponse* response) override {
    if (request->session_id() != session_id_) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "Unknown session_id");
    }
    if (fail_interpolate_) {
      return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Mock OOM");
    }

    last_chunk_req_ = *request;
    const size_t num_in = static_cast<size_t>(request->num_input_frames());
    const size_t num_out = num_in * temporal_sf_;

    // Repeat each input frame `temporal_sf_` times into the output SHM.
    for (size_t i = 0; i < num_in; ++i) {
      const uint8_t* src = in_shm_->data() + i * frame_bytes_;
      for (size_t r = 0; r < temporal_sf_; ++r) {
        uint8_t* dst = out_shm_->data() + (i * temporal_sf_ + r) * frame_bytes_;
        std::memcpy(dst, src, frame_bytes_);
      }
    }

    response->set_chunk_index(request->chunk_index());
    response->set_num_output_frames(static_cast<int32_t>(num_out));
    response->set_inference_time_ms(12.5);
    return grpc::Status::OK;
  }

  grpc::Status DestroySession(
      grpc::ServerContext* context,
      const viduce::ldfvfi::DestroySessionRequest* request,
      viduce::ldfvfi::DestroySessionResponse* response) override {
    if (request->session_id() != session_id_) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "Unknown session_id");
    }
    destroy_called_ = true;
    in_shm_.reset();
    out_shm_.reset();
    session_id_.clear();
    return grpc::Status::OK;
  }

  bool fail_create_ = false;
  bool fail_interpolate_ = false;
  bool destroy_called_ = false;
  viduce::ldfvfi::CreateSessionRequest last_create_req_;
  viduce::ldfvfi::InterpolateChunkRequest last_chunk_req_;

 private:
  int session_counter_ = 0;
  std::string session_id_;
  size_t frame_bytes_ = 0;
  size_t temporal_sf_ = 4;
  std::unique_ptr<ShmBuffer> in_shm_;
  std::unique_ptr<ShmBuffer> out_shm_;
};

absl::StatusOr<std::unique_ptr<Frame>> MakeSolidRgbFrame(int width, int height,
                                                         uint8_t value) {
  absl::StatusOr<std::unique_ptr<Frame>> frame =
      Frame::Create(StreamIndex{0}, AVMEDIA_TYPE_VIDEO);
  if (!frame.ok()) {
    return frame.status();
  }
  AVFrame* av_frame = (*frame)->frame();
  av_frame->width = width;
  av_frame->height = height;
  av_frame->format = AV_PIX_FMT_RGB24;
  if (av_frame_get_buffer(av_frame, /*align=*/0) < 0) {
    return absl::InternalError("Failed to allocate AVFrame buffer");
  }
  for (int y = 0; y < height; ++y) {
    std::memset(av_frame->data[0] + y * av_frame->linesize[0], value,
                width * kRgbChannels);
  }
  return frame;
}

TEST(LdfVfiClientTest, FullLifecycleInitInterpolateAndClose) {
  FakeLdfVfiService service;
  grpc::ServerBuilder builder;
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  LdfVfiClient client(server->InProcessChannel(grpc::ChannelArguments()));
  InterpolationConfig config{
      .width = 8,
      .height = 6,
      .temporal_scale_factor = 2,
      .sampling_steps = 10,
      .stream_index = StreamIndex{1},
      .output_pix_fmt = AV_PIX_FMT_RGB24,
  };
  ASSERT_THAT(client.Init(config), IsOk());
  EXPECT_EQ(service.last_create_req_.media_metadata().width(), 8);
  EXPECT_EQ(service.last_create_req_.media_metadata().height(), 6);
  EXPECT_EQ(service.last_create_req_.params().temporal_scale_factor(), 2);
  EXPECT_EQ(service.last_create_req_.params().sampling_steps(), 10);

  auto f0 = MakeSolidRgbFrame(8, 6, 50);
  auto f1 = MakeSolidRgbFrame(8, 6, 150);
  ABSL_ASSERT_OK(f0);
  ABSL_ASSERT_OK(f1);

  absl::StatusOr<std::vector<std::unique_ptr<Frame>>> out =
      client.InterpolateChunk({f0->get(), f1->get()}, /*is_last_chunk=*/true);
  ABSL_ASSERT_OK(out);
  ASSERT_EQ(out->size(), 4);
  EXPECT_EQ(service.last_chunk_req_.chunk_index(), 0);
  EXPECT_EQ(service.last_chunk_req_.num_input_frames(), 2);
  EXPECT_TRUE(service.last_chunk_req_.is_last_chunk());

  EXPECT_EQ((*out)[0]->frame()->data[0][0], 50);
  EXPECT_EQ((*out)[1]->frame()->data[0][0], 50);
  EXPECT_EQ((*out)[2]->frame()->data[0][0], 150);
  EXPECT_EQ((*out)[3]->frame()->data[0][0], 150);

  EXPECT_THAT(client.Close(), IsOk());
  EXPECT_TRUE(service.destroy_called_);

  server->Shutdown();
}

TEST(LdfVfiClientTest, PropagatesGrpcErrorsAndValidatesConfig) {
  FakeLdfVfiService service;
  grpc::ServerBuilder builder;
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  ASSERT_NE(server, nullptr);

  LdfVfiClient client(server->InProcessChannel(grpc::ChannelArguments()));

  // Invalid config dimensions.
  EXPECT_THAT(client.Init({.width = 0, .height = 8}),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // InterpolateChunk before Init fails.
  auto f0 = MakeSolidRgbFrame(8, 6, 10);
  ABSL_ASSERT_OK(f0);
  EXPECT_THAT(client.InterpolateChunk({f0->get()}, false),
              StatusIs(absl::StatusCode::kFailedPrecondition));

  // Server failure during CreateSession.
  service.fail_create_ = true;
  EXPECT_THAT(client.Init({.width = 8, .height = 6}),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Mock create failure")));
  service.fail_create_ = false;

  // Successful Init followed by OOM during InterpolateChunk.
  ASSERT_THAT(client.Init({.width = 8, .height = 6}), IsOk());
  service.fail_interpolate_ = true;
  EXPECT_THAT(client.InterpolateChunk({f0->get()}, false),
              StatusIs(absl::StatusCode::kResourceExhausted,
                       HasSubstr("Mock OOM")));

  EXPECT_THAT(client.Close(), IsOk());
  server->Shutdown();
}

}  // namespace
}  // namespace viduce::engine::interpolate
