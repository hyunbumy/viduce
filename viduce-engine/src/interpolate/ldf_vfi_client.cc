#include "engine/interpolate/ldf_vfi_client.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "engine/frame.h"
#include "engine/interpolate/interpolator.h"
#include "engine/interpolate/shm_buffer.h"
#include "engine/interpolate/shm_frame_packer.h"
#include "engine/interpolate/shm_frame_unpacker.h"
#include "grpcpp/grpcpp.h"
#include "ldfvfi/v1/service.grpc.pb.h"
#include "ldfvfi/v1/service.pb.h"

namespace viduce::engine::interpolate {

namespace {

constexpr int kRgbChannels = 3;
constexpr auto kRpcTimeout = std::chrono::seconds(60);

absl::Status GrpcStatusToAbsl(const grpc::Status& status) {
  if (status.ok()) {
    return absl::OkStatus();
  }
  return absl::Status(static_cast<absl::StatusCode>(status.error_code()),
                      status.error_message());
}

}  // namespace

LdfVfiClient::LdfVfiClient(std::shared_ptr<grpc::ChannelInterface> channel)
    : stub_(viduce::ldfvfi::LdfVfiService::NewStub(std::move(channel))) {}

LdfVfiClient::~LdfVfiClient() {
  if (!session_id_.empty()) {
    (void)Close();
  }
}

absl::Status LdfVfiClient::Init(const InterpolationConfig& config) {
  if (config.width <= 0 || config.height <= 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid frame dimensions: %dx%d", config.width, config.height));
  }
  if (config.temporal_scale_factor <= 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid temporal_scale_factor: %d", config.temporal_scale_factor));
  }
  if (config.sampling_steps <= 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid sampling_steps: %d", config.sampling_steps));
  }

  if (!session_id_.empty()) {
    (void)Close();
  }

  viduce::ldfvfi::CreateSessionRequest request;
  auto* metadata = request.mutable_media_metadata();
  metadata->set_width(config.width);
  metadata->set_height(config.height);
  metadata->set_channels(kRgbChannels);
  metadata->set_pixel_format(viduce::ldfvfi::PIXEL_FORMAT_RGB24);

  auto* params = request.mutable_params();
  params->set_temporal_scale_factor(config.temporal_scale_factor);
  params->set_sampling_steps(config.sampling_steps);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + kRpcTimeout);
  viduce::ldfvfi::CreateSessionResponse response;
  grpc::Status rpc_status = stub_->CreateSession(&context, request, &response);
  if (!rpc_status.ok()) {
    return GrpcStatusToAbsl(rpc_status);
  }
  if (response.session_id().empty()) {
    return absl::InternalError(
        "CreateSession returned an empty session_id from daemon");
  }

  const std::string session_id = response.session_id();
  const size_t min_frame_bytes = static_cast<size_t>(config.width) *
                                 static_cast<size_t>(config.height) *
                                 kRgbChannels;

  const std::string in_shm_name = absl::StrCat("/viduce_", session_id, "_in");
  const std::string out_shm_name = absl::StrCat("/viduce_", session_id, "_out");

  absl::StatusOr<std::unique_ptr<ShmBuffer>> input_shm =
      ShmBuffer::Open(in_shm_name, /*min_size=*/0, /*read_only=*/false);
  if (!input_shm.ok()) {
    return input_shm.status();
  }
  if ((*input_shm)->size() < min_frame_bytes) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Input SHM '%s' size (%zu) is smaller than a single frame (%zu)",
        in_shm_name, (*input_shm)->size(), min_frame_bytes));
  }

  absl::StatusOr<std::unique_ptr<ShmBuffer>> output_shm =
      ShmBuffer::Open(out_shm_name, /*min_size=*/0, /*read_only=*/true);
  if (!output_shm.ok()) {
    return output_shm.status();
  }
  if ((*output_shm)->size() < min_frame_bytes) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Output SHM '%s' size (%zu) is smaller than a single frame (%zu)",
        out_shm_name, (*output_shm)->size(), min_frame_bytes));
  }

  session_id_ = session_id;
  chunk_index_ = 0;
  input_shm_ = std::move(*input_shm);
  output_shm_ = std::move(*output_shm);
  packer_ = std::make_unique<ShmFramePacker>(config.width, config.height);
  unpacker_ = std::make_unique<ShmFrameUnpacker>(
      config.width, config.height, config.stream_index, config.output_pix_fmt);
  return absl::OkStatus();
}

absl::StatusOr<std::vector<std::unique_ptr<Frame>>>
LdfVfiClient::InterpolateChunk(const std::vector<Frame*>& input_frames,
                               bool is_last_chunk) {
  if (session_id_.empty() || input_shm_ == nullptr || output_shm_ == nullptr) {
    return absl::FailedPreconditionError(
        "LdfVfiClient session is not initialized");
  }
  if (input_frames.empty()) {
    return absl::InvalidArgumentError("input_frames cannot be empty");
  }

  absl::Status pack_status =
      packer_->PackFrames(input_frames, input_shm_.get());
  if (!pack_status.ok()) {
    return pack_status;
  }

  viduce::ldfvfi::InterpolateChunkRequest request;
  request.set_session_id(session_id_);
  request.set_chunk_index(chunk_index_);
  request.set_num_input_frames(static_cast<int32_t>(input_frames.size()));
  request.set_is_last_chunk(is_last_chunk);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + kRpcTimeout);
  viduce::ldfvfi::InterpolateChunkResponse response;
  grpc::Status rpc_status =
      stub_->InterpolateChunk(&context, request, &response);
  if (!rpc_status.ok()) {
    return GrpcStatusToAbsl(rpc_status);
  }
  if (response.num_output_frames() <= 0) {
    return absl::InternalError(absl::StrFormat(
        "Daemon returned non-positive num_output_frames: %d",
        response.num_output_frames()));
  }

  ++chunk_index_;
  return unpacker_->UnpackFrames(
      *output_shm_, static_cast<size_t>(response.num_output_frames()));
}

absl::Status LdfVfiClient::Close() {
  input_shm_.reset();
  output_shm_.reset();
  packer_.reset();
  unpacker_.reset();

  if (session_id_.empty()) {
    return absl::OkStatus();
  }

  const std::string sid = std::move(session_id_);
  session_id_.clear();

  viduce::ldfvfi::DestroySessionRequest request;
  request.set_session_id(sid);
  viduce::ldfvfi::DestroySessionResponse response;
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + kRpcTimeout);
  grpc::Status rpc_status = stub_->DestroySession(&context, request, &response);
  return GrpcStatusToAbsl(rpc_status);
}

}  // namespace viduce::engine::interpolate
