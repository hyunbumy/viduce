# Project Detail

Common project context for AI agents working in this repository.

## Overview

Viduce is a video compression + AI enhancement system. It downscales video resolution and reduces FPS for compact storage, then uses ML-based upscaling (Real-ESRGAN via TFLite/LiteRT) to restore quality on playback.

The system is a Rust workspace (CLI, ffmpeg wrapping, cloud/local upscalers) + a C++ engine (libav frame decoding + LiteRT inference), linked via a C FFI shared library.

---

## Build Commands

### Prerequisites

The C++ engine **must be built before** `cargo build` — it produces `engine/build/lib/libengine_api.so` which Rust links against.

### C++ Engine

```bash
cd engine
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j8
cd build && ctest -VV          # All C++ tests (verbose)
ctest -R <test_name> -VV       # Single C++ test
```

### Rust (workspace root)

```bash
cargo build                              # Debug build
cargo build --release                    # Release build
cargo test                               # All Rust tests
cargo test --package <crate> <name>      # Single test, e.g.:
cargo test --package ffmpeg execute_success
cargo test --package upscaler upscale_success_returns_upscaled_bytes
```

### Development Container

```bash
./start_devenv.sh    # Builds base Docker image and runs container with repo volume-mounted
```

### Docker

```bash
docker build -t viduce .                                # Debug image (default)
docker build --build-arg BUILD_TYPE=release -t viduce . # Release image
```

The Dockerfile `build` stage runs `ctest -VV` and `cargo test` — a successful Docker build implies all tests passed.

### Environment Variables

- **`VIDUCE_UPSCALER_PATH`** — absolute path to the `.tflite` model file; required by `EnhanceVideo` in the C++ engine.
- **`SEGMIND_API_KEY`** — API key for the Segmind cloud upscaler; read by `cmd::UpscalerRunner`.
- **`LD_LIBRARY_PATH`** — must include the directory containing `libengine_api.so` (and FFmpeg/LiteRT runtime deps) when running the `engine` subcommand outside Docker.

---

## Architecture

### Rust Workspace Crates

<!-- TODO: Move Rust code into its separate package. -->

```
cmd  ->  ffmpeg, upscaler, util, libc (+ links libengine_api.so via build.rs)
ffmpeg -> util
upscaler -> util, reqwest (blocking), tokio, mockito
util -> (stdlib only)
```

#### `util`
- **`ProcessRunner` trait**: `run(&mut self, program: &str, args: &[&str]) -> io::Result<()>`. `DefaultRunner` executes via `std::process::Command`, pipes stdout/stderr, returns error if exit code is non-zero.
- **`UriIo` trait**: `read(uri, buf)` / `write(uri, buf)`. `UriHandler` treats every URI as a local file path; URL schemes are not yet handled.

#### `ffmpeg`
- **`FfmpegCommand`**: builder struct with `input`, `output: String`, `resolution: Option<Resolution>`, `fps: Option<u32>`. `Resolution` enum: `R144P=144` … `R1080P=1080`. `new()` rejects empty input/output.
- **`FfmpegWrapper<'a>`**: wraps a `&mut dyn ProcessRunner`. `execute(command)` builds ffmpeg args:
  - Always: `ffmpeg -y -i assets/input/{input}`
  - If resolution set: `-vf scale=-2:{res_value}`
  - If fps set: `-r {fps}`
  - Output: `assets/output/{output}`

#### `upscaler`
- **`Upscaler` trait**: `upscale(&mut self, input_uri: &str, output_url: &str) -> Result<(), Box<dyn Error>>`.
- **`SegmindUpscaler`**: POSTs to `https://api.segmind.com/v1/esrgan-video-upscaler` with `x-api-key` header and JSON body `{ crop_to_fit: "False", input_video: <uri>, res_model: "RealESRGAN_x4plus", resolution: "FHD" }`. Writes response bytes via `UriIo`.
- **`RealEsrganUpscaler<T: ProcessRunner>`**: three-step local pipeline:
  1. `ffmpeg -i <input> -c:v png assets/temp/original/frame_%000d.png` — extract frames
  2. `bin/realesrgan/model_runner -i assets/temp/original -o assets/temp/generated` — upscale
  3. `ffmpeg -r 1 -i assets/temp/generated/frame_%000d.png -vcodec libsvtav1 -crf 35 -r 30 <output>` — reassemble (output FPS hardcoded to 30)

#### `cmd`
- **`main.rs`**: expects exactly 1 CLI arg; routes `ffmpeg` / `upscaler` / `realesrgan` / `engine` to the respective runner.
- **`FfmpegRunner`**: interactive REPL — prompts user for input, output, fps, resolution.
- **`UpscalerRunner`**: reads `SEGMIND_API_KEY` env var; `VIDEO_URL` is hardcoded to a sample S3 URL. Prompts user for output filename written to `assets/output/`.
- **`RealEsrganRunner`**: hardcodes input `assets/input/input_480_1fps.mp4` and output `assets/output/output.mp4`.
- **`EngineRunner`**: calls `DecodeVideo(CString::new("assets/input/input.mp4"))` via FFI. Note: `DecodeVideo` is deprecated in C++ (returns Unimplemented); `EnhanceVideo` is implemented in C++ but **not yet declared in the Rust FFI block**.
- **`build.rs`**: `println!("cargo:rustc-link-search=engine/build/lib")` — tells Cargo where to find `libengine_api.so`.

### C++ Engine (`engine/`)

#### C FFI surface (`engine/include/engine/engine_api.h`)
```cpp
extern "C" {
int DecodeVideo(const char* input_path);                          // deprecated
int EnhanceVideo(const char* input_path, const char* output_dir); // active
}
```
Returns `absl::Status::raw_code()` (0 = OK). `VIDUCE_UPSCALER_PATH` env var must be set for `EnhanceVideo`.

#### Frame layer
- **`Frame`**: RAII wrapper around `AVFrame*`. `Frame::Create(StreamInfo)` allocates via `av_frame_alloc`.
- **`FrameReader`**: wraps `AVFormatContext*` + `vector<AVCodecContext*>`.
  - `Create(url)` opens and finds stream info; sets up one codec context per stream.
  - `ReadNextFrame()` → `StatusOr<unique_ptr<Frame>>`: returns `nullptr` at EOF.

#### Upscaling layer (`engine/src/upscale/`)
- **`Model` (abstract)**: `RunModel(ModelIo) -> StatusOr<ModelIo>`. `ModelIo` is a normalized float vector layout `(channel, height, width)` in `[0, 1]`.
- **`ModelImpl`**: wraps `litert::CompiledModel` (CPU). Hardcoded to 240×240 input. Returns outputs clamped to `[0, 1]`.
- **`Upscaler`** (C++): takes a `Model*`.
  - `Upscale(Frame*)`: converts to GBR24P → float `ModelIo` → `RunModel` → float `ModelIo` → GBR24P at `scale×` dimensions → convert back to source pixel format.
  - `scale` is fixed at 4× (from `ModelImpl::kUpscale`).

#### Engine API implementation
- **`EnhanceVideo`** internal pipeline:
  1. Reads `VIDUCE_UPSCALER_PATH`, creates `ModelImpl` + `Upscaler` + `FrameReader`.
  2. Loops `ReadNextFrame()` → `Upscaler::Upscale()`.
  3. Each upscaled frame written to `{output_dir}/frame_{i}.ppm` (RGB24, P6 PPM format).
- **`WriteToOutput`**: converts frame to RGB24 via `sws_scale`, writes raw P6 PPM.

#### C++ dependency graph
```
engine_api (shared) -> frame_reader, model_impl, upscaler_cc, absl, spdlog, avcodec, avutil
frame_reader        -> frame, util_cc, absl, av*, swscale
model_impl          -> litert (prebuilt libLiteRt.so v2.1.2 + SDK v2.1.1), avutil
upscaler_cc         -> frame, util_cc, absl
frame               -> avutil, absl::statusor
```

External C++ libs (via apt): `libavformat`, `libavcodec`, `libavutil`, `libswscale`. Note: `engine/deps/ffmpeg.cmake` defines an in-tree FFmpeg build but is not included by the deps CMakeLists.txt — system FFmpeg is used instead.

### Model Pipeline

`models/convert_model.py` converts a PyTorch Real-ESRGAN checkpoint to TFLite format:
- Architecture: `RRDBNet(num_in_ch=3, num_out_ch=3, num_feat=64, num_block=23, num_grow_ch=32, scale=4)`.
- Sample input shape: `(1, 3, 240, 240)` — the TFLite model is hardcoded to this input dimension.
- Uses `litert_torch.convert` + `.export(output_path)`.
- Converted models are **not checked in** — generate locally, then point `VIDUCE_UPSCALER_PATH` at the result.
- Python deps managed by `uv`: `basicsr-fixed>=1.4.2`, `litert-torch>=0.8.0`, `torch>=2.9.0`.

---

## Testing

### Rust tests
- `ffmpeg` crate: uses `MockCommandRunner` (records last command, injects errors). Tests verify exact ffmpeg arg strings.
- `upscaler` crate: `SegmindUpscaler` uses `mockito::Server` for HTTP; `MockUriHandler` captures written bytes.
- No tests for `cmd` runners.

### C++ tests (gtest + gmock, with AddressSanitizer)
- `frame_test`: validates `Frame::Create` with a `StreamInfo` struct.
- `frame_reader_test`: integration tests against `data/test.mp4` and `data/3frames_240x240.mp4` (git-lfs tracked). Verifies `ReadNextFrame` yields exactly 3 frames from the 3-frame fixture.
- `upscale/upscaler_test`: uses `MockModel` (scale=2, echoes input data quadrupled). Verifies pixel values are preserved through the full color conversion round-trip; also tests model error propagation.
- No tests for `ModelImpl` (requires real .tflite) or `engine_api.cc` orchestration.

---

## Key Caveats

- **`EnhanceVideo` not wired to Rust**: the `EngineRunner` calls the deprecated `DecodeVideo` (which returns `Unimplemented`). The only working path for `EnhanceVideo` is the C++ `demo` binary (`engine/src/demo.cc`).
- **`ffmpeg_wrapper.rs` path prefixing**: `FfmpegWrapper` prepends `assets/input/`/`assets/output/` — user-supplied paths must be relative to those directories.
- **RealEsrganUpscaler FPS**: output is hardcoded to 30 FPS regardless of input.
- **Model input shape**: `ModelImpl` is hardcoded to 240×240. Frames of other sizes will not work correctly.
- **CI**: builds debug Docker image only, cache-only output (no push). `git lfs pull` is run explicitly after checkout.
