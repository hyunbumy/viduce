use util::process_runner::{DefaultRunner, ProcessRunner};

pub struct RealEsrganRunner {}

impl RealEsrganRunner {
    pub fn new() -> Self {
        RealEsrganRunner {}
    }

    pub fn run(&mut self) {
        // 1. use ffmpeg to split video into frames
        let args = vec![
            "-i",
            "assets/input/input_480.mp4",
            "-vf",
            "fps=1",
            "assets/temp/original/frame_%000d.png",
        ];
        if let Err(e) = DefaultRunner::new().run("ffmpeg", &args) {
            print!("Failed to split video into frames: {}", e);
            return;
        }

        // 2. use realesrgan to upscale frames
        let args = vec!["-i", "assets/temp/original", "-o", "assets/temp/generated"];
        if let Err(e) = DefaultRunner::new().run("bin/realesrgan/model_runner", &args) {
            print!("Failed to upscale frames: {}", e);
            return;
        }

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
        if let Err(e) = DefaultRunner::new().run("ffmpeg", &args) {
            print!("Failed to merge frames into a video: {}", e);
            return;
        }

        println!("Successfully upscaled video");
    }
}
