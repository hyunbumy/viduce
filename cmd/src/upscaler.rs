use std::env;
use std::io;
use std::io::Write;
use std::fs::File;
use upscaler::Upscaler;

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
        let mut upscaler = upscaler::SegmindUpscaler::new(&api_key);
        let result = match upscaler.upscale(VIDEO_URL) {
            Ok(res) => res,
            Err(e) => {
                println!("Failed to run upscaler with error: {}", e);
                return;
            }
        };

        // Write result to output
        println!("Writing output...");
        let mut file = match File::create("assets/output/".to_owned() + output) {
            Ok(file) => file,
            Err(e) => {
                println!("Failed to open file: {}", e);
                return;
            }
        };
        if let Err(e) = file.write_all(&result) {
            println!("Failed to write to an output: {}", e);
        }
    }
}
