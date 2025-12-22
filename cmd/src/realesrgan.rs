use upscaler::{RealEsrganUpscaler, Upscaler};
use util::process_runner::DefaultRunner;

pub struct RealEsrganRunner {}

impl RealEsrganRunner {
    pub fn new() -> Self {
        RealEsrganRunner {}
    }

    pub fn run(&mut self) {
        let mut runner = DefaultRunner::new();
        let mut upscaler = RealEsrganUpscaler::new(&mut runner);
        if let Err(e) = upscaler.upscale(
            "assets/input/input_480_1fps.mp4",
            "assets/output/output.mp4",
        ) {
            println!("Failed to run upscaler with error: {}", e);
            return;
        };
    }
}
