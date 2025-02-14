mod command_runner;
mod ffmpeg;

use command_runner::CommandRunner;
use ffmpeg::FfmpegCommand;
use ffmpeg::FfmpegWrapper;
use ffmpeg::Resolution;
use std::io::Write;
use std::{
    io::{self, Result},
    process::Command,
};

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
            println!("Please specify the input:");
            let mut input = String::new();
            io::stdin().read_line(&mut input).unwrap();
            let input = input.trim_end();

            println!("");

            println!("Please specify the output:");
            let mut output = String::new();
            io::stdin().read_line(&mut output).unwrap();
            let output = output.trim_end();

            let mut cmd = match FfmpegCommand::new(input, output) {
                Ok(cmd) => cmd,
                Err(e) => {
                    println!("{e}");
                    continue;
                }
            };

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
}

struct DefaultRunner {}

impl DefaultRunner {
    fn new() -> DefaultRunner {
        DefaultRunner {}
    }
}

impl CommandRunner for DefaultRunner {
    fn run(&mut self, program: &str, args: &[String]) -> Result<()> {
        let output = Command::new(program).args(args).output()?;

        // TODO: Log this explicitly instead of stdout
        io::stdout().write_all(&output.stdout)?;
        io::stderr().write_all(&output.stderr)?;

        if !output.status.success() {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                format!(
                    "Failed to execute with status {}",
                    output.status.to_string()
                ),
            ));
        }

        Ok(())
    }
}
