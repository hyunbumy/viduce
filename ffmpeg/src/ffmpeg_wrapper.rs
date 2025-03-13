use crate::ffmpeg_command::FfmpegCommand;
use std::io;
use util::process_runner::ProcessRunner;

pub struct FfmpegError {
    pub msg: String,
}

impl FfmpegError {
    pub fn from_io_error(e: io::Error) -> FfmpegError {
        FfmpegError { msg: e.to_string() }
    }
}

// Library for interacting with ffmpeg
// TODO: Flesh out the API
// TODO: Eventually consider having a long-running ffmpeg instance.
pub struct FfmpegWrapper<'a> {
    runner: Box<&'a mut dyn ProcessRunner>,
}

impl<'a> FfmpegWrapper<'a> {
    // Creates a new instance
    pub fn new(runner: Box<&'a mut dyn ProcessRunner>) -> Self {
        FfmpegWrapper { runner }
    }

    pub fn execute(&mut self, command: FfmpegCommand) -> Result<(), FfmpegError> {
        let input = format!("assets/input/{}", command.input);
        let mut args = vec![
            "-y", // Force overwrite
            "-i", // Set input
            input.as_str(),
        ];

        let mut res_str = None;
        if let Some(res) = command.resolution {
            args.push("-vf");
            let temp = res_str.insert(format!("scale=-2:{}", res as i32));
            args.push(temp.as_str());
        }

        let mut fps_str = None;
        if let Some(fps) = command.fps {
            args.push("-r");
            let temp = fps_str.insert(format!("{}", fps));
            args.push(temp.as_str());
        }

        let output = format!("assets/output/{}", command.output);
        args.push(output.as_str());

        self.runner
            .run("ffmpeg", &args)
            .map_err(FfmpegError::from_io_error)?;

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ffmpeg_command::Resolution;

    struct MockCommandRunner {
        last_run_command: String,
        error: Option<io::Error>,
    }

    impl MockCommandRunner {
        fn new() -> MockCommandRunner {
            MockCommandRunner {
                last_run_command: String::new(),
                error: None,
            }
        }

        fn new_with_error(error: io::Error) -> MockCommandRunner {
            MockCommandRunner {
                last_run_command: String::new(),
                error: Some(error),
            }
        }
    }

    impl ProcessRunner for MockCommandRunner {
        fn run(&mut self, program: &str, args: &[&str]) -> io::Result<()> {
            self.last_run_command = format!("{program} {}", args.join(" "));
            if let Some(err) = &self.error {
                return Err(io::Error::new(err.kind(), format!("{err}")));
            }
            Ok(())
        }
    }

    #[test]
    fn execute_success() {
        let input = "input.mp4";
        let output = "output.mp4";
        let command = FfmpegCommand::new(input, output).unwrap();

        let mut mock_runner = MockCommandRunner::new();
        let mut ffmpeg_wrapper = FfmpegWrapper::new(Box::new(&mut mock_runner));
        let result = ffmpeg_wrapper.execute(command);

        assert!(result.is_ok());
        assert_eq!(
            mock_runner.last_run_command,
            "ffmpeg -y -i assets/input/input.mp4 assets/output/output.mp4"
        );
    }

    #[test]
    fn execute_fail() {
        let input = "input.mp4";
        let output = "output.mp4";
        let command = FfmpegCommand::new(input, output).unwrap();

        let mut mock_runner =
            MockCommandRunner::new_with_error(io::Error::new(io::ErrorKind::Other, "test error"));
        let mut ffmpeg_wrapper = FfmpegWrapper::new(Box::new(&mut mock_runner));
        let result = ffmpeg_wrapper.execute(command);

        assert!(result.is_err_and(|e| e.msg.contains("test error")));
    }

    #[test]
    fn execute_with_resolution_success() {
        let input = "input.mp4";
        let output = "output.mp4";
        let mut command = FfmpegCommand::new(input, output).unwrap();
        command.set_resolution(Resolution::R480P);

        let mut mock_runner = MockCommandRunner::new();
        let mut ffmpeg_wrapper = FfmpegWrapper::new(Box::new(&mut mock_runner));
        let result = ffmpeg_wrapper.execute(command);

        assert!(result.is_ok());
        assert_eq!(
            mock_runner.last_run_command,
            "ffmpeg -y -i assets/input/input.mp4 -vf scale=-2:480 assets/output/output.mp4"
        );
    }

    #[test]
    fn execute_with_fps_success() {
        let input = "input.mp4";
        let output = "output.mp4";
        let mut command = FfmpegCommand::new(input, output).unwrap();
        command.set_fps(24);

        let mut mock_runner = MockCommandRunner::new();
        let mut ffmpeg_wrapper = FfmpegWrapper::new(Box::new(&mut mock_runner));
        let result = ffmpeg_wrapper.execute(command);

        assert!(result.is_ok());
        assert_eq!(
            mock_runner.last_run_command,
            "ffmpeg -y -i assets/input/input.mp4 -r 24 assets/output/output.mp4"
        );
    }
}
