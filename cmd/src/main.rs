use cmd::{FfmpegCommand, FfmpegWrapper, Resolution};
use std::io::{self};

// TODO(hyunbumy): Figure out how we would package ffmpeg binary
fn main() {
    let ffmpeg = FfmpegWrapper::new();

    // TODO(hyunbumy): Add graceful exit;
    loop {
        println!("Please specify the input:");
        let mut input = String::new();
        io::stdin().read_line(&mut input).unwrap();
        let input = input.trim_end();

        println!("");

        println!("Please specify the output:");
        let mut output = String::new();
        io::stdin().read_line(&mut output).unwrap();
        let output = output.trim_end();

        let mut cmd = FfmpegCommand::new(input, output);
        cmd.set_fps(1);
        cmd.set_resolution(Resolution::R480P);
        if let Err(e) = ffmpeg.execute(cmd) {
            println!("Error executing ffmpeg command: {}", e.msg);
        }

        println!("\n\n");
        println!("=====================");
        println!("\n");
    }
}
