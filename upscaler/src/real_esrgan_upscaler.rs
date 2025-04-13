use crate::upscaler::Upscaler;
use std::error::Error;
use util::process_runner::ProcessRunner;

pub struct RealEsrganUpscaler<'a, T: ProcessRunner> {
    runner: &'a mut T,
}

impl<'a, T: ProcessRunner> RealEsrganUpscaler<'a, T> {
    pub fn new(runner: &'a mut T) -> Self {
        RealEsrganUpscaler { runner }
    }
}

impl<'a, T: ProcessRunner> Upscaler for RealEsrganUpscaler<'a, T> {
    fn upscale(&mut self, input_uri: &str, output_uri: &str) -> Result<(), Box<dyn Error>> {
        // 1. use ffmpeg to split video into frames
        // TODO: Decode to raw frames instead. We can extract this into a common
        // node in the processing pipeline to reuse the decoded frames.
        let args = vec![
            "-i",
            input_uri,
            "-c:v",
            "png",
            "assets/temp/original/frame_%000d.png",
        ];
        self.runner.run("ffmpeg", &args)?;

        // 2. use realesrgan to upscale frames
        let args = vec!["-i", "assets/temp/original", "-o", "assets/temp/generated"];
        self.runner.run("bin/realesrgan/model_runner", &args)?;

        // 3. use ffmpeg to merge frames into video
        let args = vec![
            "-r",
            "1", // TODO: Read the input fps and store so that we can restore it after upscaling.
            "-i",
            "assets/temp/generated/frame_%000d.png",
            "-vcodec",
            "libsvtav1",
            "-crf",
            "35",
            "-r",
            "30", // Explicitly set the output fps to 30 to avoid low FPS playback issues with players.
            output_uri,
        ];
        self.runner.run("ffmpeg", &args)?;

        Ok(())
    }
}
