use crate::upscaler::Upscaler;
use std::error::Error;
use util::process_runner::DefaultRunner;
use util::process_runner::ProcessRunner;

struct RealEsrganUpscaler {}

impl Upscaler for RealEsrganUpscaler {
    fn upscale(&mut self, uri: &str) -> Result<Vec<u8>, Box<dyn Error>> {
        // 1. use ffmpeg to split video into frames
        // TODO: Decode to raw frames instead. We can extract this into a common
        // node in the processing pipeline to reuse the decoded frames.
        let args = vec![
            "-i",
            uri,
            "-c:v",
            "png",
            "assets/temp/original/frame_%000d.png",
        ];
        DefaultRunner::new().run("ffmpeg", &args)?;

        // 2. use realesrgan to upscale frames
        let args = vec!["-i", "assets/temp/original", "-o", "assets/temp/generated"];
        DefaultRunner::new().run("bin/realesrgan/model_runner", &args)?;

        // 3. use ffmpeg to merge frames into video
        let args = vec![
            "-r",
            "1",
            "-i",
            "assets/temp/generated/frame_%000d.png",
            "-vcodec",
            "libsvtav1",
            "-crf",
            "35",
            "-r",
            "30", // Explicitly set the output fps to 30 to avoid low FPS playback issues with players.
            "assets/output/output.mp4",
        ];
        DefaultRunner::new().run("ffmpeg", &args)?;

        return Ok(Vec::new());
    }
}
