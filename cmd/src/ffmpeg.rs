use ffmpeg::wrapper::command::FfmpegCommand;
use ffmpeg::wrapper::FfmpegWrapper;
use std::io;
use util::process_runner::DefaultRunner;

fn parse_to_fps(input: &str) -> Option<u32> {
    match input.parse() {
        Ok(fps) => Some(fps),
        Err(_) => None,
    }
}

pub struct FfmpegRunner {}

impl FfmpegRunner {
    pub fn new() -> FfmpegRunner {
        FfmpegRunner {}
    }

    pub fn run(&self) {
        let mut runner = DefaultRunner::new();
        let mut ffmpeg = FfmpegWrapper::new(Box::new(&mut runner));

        // TODO(hyunbumy): Add graceful exit;
        loop {
            let input = Self::get_user_input("Please specify the input:");
            let output = Self::get_user_input("Please specify the output:");

            let mut cmd = match FfmpegCommand::new(&input, &output) {
                Ok(cmd) => cmd,
                Err(e) => {
                    println!("{e}");
                    continue;
                }
            };

            if let Some(fps) = parse_to_fps(&Self::get_user_input("Specify FPS:")) {
                cmd.set_fps(fps);
            }

            if let Some(resolution) = ffmpeg::wrapper::command::parse_to_resolution(
                &Self::get_user_input("Specify resolution:"),
            ) {
                cmd.set_resolution(resolution);
            }

            println!("Executing command");
            if let Err(e) = ffmpeg.execute(cmd) {
                println!("Error executing ffmpeg command: {}", e.msg);
            }

            println!("\n\n");
            println!("=====================");
            println!("\n");
        }
    }

    fn get_user_input(prompt: &str) -> String {
        println!("{prompt}");
        let mut user_input = String::new();
        io::stdin().read_line(&mut user_input).unwrap();
        println!("");
        String::from(user_input.trim())
    }
}
