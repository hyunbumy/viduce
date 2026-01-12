#include "engine_api.h"

// Initial cc implementation for accepting an input video and outputing
// individual frames.
int main(int argc, char** argv) {
  decode_video("input.mp4", "output_frames/");
  return 0;
}
