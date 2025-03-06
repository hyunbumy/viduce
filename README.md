# Compress videos

## TODOs
### Milestone 1
1. Write a wrapper around ffmpeg using Rust
    - May need Docker for env setup?
1. IO for reading provided video via ffmpeg
1. API for processing using ffmpeg

### Milestone 2 - Basic upload + play functionalities
* Upload
    * Process - downscale, lower fps
    * Store
* Playback
    * Process - upscale
    * Play

### Milestone 3 - Real-time frame interpolation + upscaling
1. Transcode to smaller video
    - Less fps
    - downscale
1. Use real-time frame interpolation and upscaling to "restore" the video
1. New upscaler using LDMVFI

### Milestone 4 - GenAI
1. Use GenAI to "describe" the video
1. Come up with a heuristics to store just enough frames to be used with the description to be able to "restore" the video
1. Use GenAI to "restore" the video using the downscaled frames + description


