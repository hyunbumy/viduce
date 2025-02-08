use std::io;
use std::io::Write;
use std::process::Command;

// Library for interacting with ffmpeg
// TODO: Flesh out the API
// TODO: Eventually consider having a long-running ffmpeg instance.
pub struct FfmpegWrapper {}

impl FfmpegWrapper {
    // Creates a new instance
    pub fn new() -> Self {
        FfmpegWrapper {}
    }

    pub fn execute(&self, command: FfmpegCommand) -> Result<(), FfmpegError> {
        let output = Command::new("ffmpeg")
            .arg("-i")
            .arg(command.input)
            .arg(command.output)
            .output()
            .map_err(FfmpegError::from_io_error)?;

        // TODO: Log this explicitly instead of stdout
        io::stdout()
            .write_all(&output.stdout)
            .map_err(FfmpegError::from_io_error)?;
        io::stderr()
            .write_all(&output.stderr)
            .map_err(FfmpegError::from_io_error)?;

        if !output.status.success() {
            return Err(FfmpegError {
                msg: output.status.to_string(),
            });
        }

        Ok(())
    }
}

pub struct FfmpegCommand {
    input: String,
    output: String,
}

impl FfmpegCommand {
    pub fn new(input: &str, output: &str) -> FfmpegCommand {
        FfmpegCommand {
            input: String::from(input),
            output: String::from(output),
        }
    }
}

pub struct FfmpegError {
    pub msg: String,
}

impl FfmpegError {
    pub fn from_io_error(e: io::Error) -> FfmpegError {
        FfmpegError { msg: e.to_string() }
    }
}
