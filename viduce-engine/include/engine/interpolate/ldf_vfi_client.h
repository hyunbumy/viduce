#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "engine/frame.h"
#include "engine/interpolate/interpolator.h"
#include "engine/interpolate/shm_buffer.h"
#include "engine/interpolate/shm_frame_packer.h"
#include "engine/interpolate/shm_frame_unpacker.h"
#include "grpcpp/grpcpp.h"
#include "ldfvfi/v1/service.grpc.pb.h"

namespace viduce::engine::interpolate {

class LdfVfiClient : public Interpolator {
 public:
  explicit LdfVfiClient(std::shared_ptr<grpc::ChannelInterface> channel);
  ~LdfVfiClient() override;

  absl::Status Init(const InterpolationConfig& config) override;

  absl::StatusOr<std::vector<std::unique_ptr<Frame>>> InterpolateChunk(
      const std::vector<Frame*>& input_frames, bool is_last_chunk) override;

  absl::Status Close() override;

 private:
  std::unique_ptr<viduce::ldfvfi::LdfVfiService::Stub> stub_;
  std::string session_id_;
  uint64_t chunk_index_ = 0;
  std::unique_ptr<ShmBuffer> input_shm_;
  std::unique_ptr<ShmBuffer> output_shm_;
  std::unique_ptr<ShmFramePacker> packer_;
  std::unique_ptr<ShmFrameUnpacker> unpacker_;
};

}  // namespace viduce::engine::interpolate
