#pragma once

extern "C" {

// Test function to call from other languages
int DecodeVideo(const char* input_path);

// Enhance the input video.
int EnhanceVideo(const char* input_path, const char* output_dir);

}  // extern "C"
