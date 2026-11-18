#pragma once

#include <vector>

#include "absl/status/status.h"
#include "engine/frame.h"
#include "engine/interpolate/shm_buffer.h"

extern "C" {
#include <libswscale/swscale.h>
}  // extern "C"

namespace viduce::engine::interpolate {

class ShmFramePacker {
 public:
  ShmFramePacker(int width, int height);
  ~ShmFramePacker();

  absl::Status PackFrames(const std::vector<Frame*>& frames, ShmBuffer* buffer);

 private:
  int width_;
  int height_;
  SwsContext* sws_ctx_ = nullptr;
};

}  // namespace viduce::engine::interpolate
