#pragma once

#include <cstdint>
#include <cstdlib>

extern "C" {

// Test function to call from other languages
int DecodeVideo(const char* input_path);

// Enhance the input video.
int EnhanceVideo(const char* input_path, const char* output_dir);

// TODO: Actual implementation
int ReceiveFrame(const uint8_t* data, size_t size);

}  // extern "C"
