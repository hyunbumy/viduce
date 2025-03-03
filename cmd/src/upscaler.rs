mod segmind_upscaler;

use std::io;

trait Upscaler {
    fn upscale(&mut self, uri: &str) -> Result<Vec<u8>, String>;
}

const SEGMIND_HOST: &str = "https://api.segmind.com";

// TODO: Get the actual video URL once we have to ability to upload actual videos.
const VIDEO_URL: &str =
    "https://segmind-sd-models.s3.amazonaws.com/display_images/video-upscale-input.mp4";

struct UpscalerRunner {}

impl UpscalerRunner {
    pub fn run(&self) -> io::Result<()> {
        // TODO: Get API key from env
        let mut upscaler = segmind_upscaler::SegmindUpscaler::new(SEGMIND_HOST, "");
        upscaler.upscale(VIDEO_URL);
        Ok(())
    }
}
