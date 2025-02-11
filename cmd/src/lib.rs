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
        let mut ffmpeg_cmd = Command::new("ffmpeg");
        ffmpeg_cmd
            .arg("-y")
            .arg("-i")
            .arg(format!("assets/input/{}",command.input));

        if let Some(res) = command.resolution {
            ffmpeg_cmd.arg("-vf");
            ffmpeg_cmd.arg(format!("scale=-2:{}", res as i32));
        }

        if let Some(fps) = command.fps {
            ffmpeg_cmd.arg("-r");
            ffmpeg_cmd.arg(format!("{}", fps as i32));
        }

        let output = ffmpeg_cmd
            .arg(format!("assets/output/{}",command.output))
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
    resolution: Option<Resolution>,
    fps: Option<u32>,
}

pub enum Resolution {
    R1080P = 1080,
    R720P = 720,
    R480P = 480,
    R240P = 240,
    R144P = 144,
}

impl FfmpegCommand {
    pub fn new(input: &str, output: &str) -> FfmpegCommand {
        FfmpegCommand {
            input: String::from(input),
            output: String::from(output),
            resolution: None,
            fps: None,
        }
    }

    // Set the target resolution of the output video
    pub fn set_resolution(&mut self, res: Resolution) -> &Self {
        self.resolution = Some(res);
        self
    }

    // set the target fps of the output video
    pub fn set_fps(&mut self, fps: u32) -> &Self {
        self.fps = Some(fps);
        self
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
