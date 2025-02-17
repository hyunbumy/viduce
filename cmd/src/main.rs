use cmd::FfmpegRunner;

// TODO(hyunbumy): Figure out how we would package ffmpeg binary
fn main() {
    let runner = FfmpegRunner::new();
    runner.run();
}
