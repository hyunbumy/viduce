use std::env;
use std::io;
use upscaler::Upscaler;
use util::uri_io::UriHandler;

// TODO: Get the actual video URL once we have to ability to upload actual videos.
const VIDEO_URL: &str =
    "https://segmind-sd-models.s3.amazonaws.com/display_images/video-upscale-input.mp4";

pub struct UpscalerRunner {}

impl UpscalerRunner {
    pub fn new() -> UpscalerRunner {
        UpscalerRunner {}
    }

    pub fn run(&self) {
        println!("Specify output: ");
        let mut user_input = String::new();
        io::stdin().read_line(&mut user_input).unwrap();
        println!("");
        let output = user_input.trim();

        let api_key = match env::var("SEGMIND_API_KEY") {
            Ok(key) => key,
            Err(e) => {
                println!("Failed to get API key with error: {}", e);
                return;
            }
        };

        println!("Upscaling...");
        let mut uri_handler = UriHandler::new();
        let mut upscaler = upscaler::SegmindUpscaler::new(&api_key, &mut uri_handler);
        if let Err(e) = upscaler.upscale(VIDEO_URL, &format!("assets/output/{output}")) {
            println!("Failed to run upscaler with error: {}", e);
            return;
        };
    }
}
