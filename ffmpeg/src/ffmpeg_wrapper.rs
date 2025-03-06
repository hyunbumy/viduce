use crate::command_runner::CommandRunner;
use crate::ffmpeg_command::FfmpegCommand;
use std::io;

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
    runner: Box<&'a mut dyn CommandRunner>,
}

impl<'a> FfmpegWrapper<'a> {
    // Creates a new instance
    pub fn new(runner: Box<&'a mut dyn CommandRunner>) -> Self {
        FfmpegWrapper { runner }
    }

    pub fn execute(&mut self, command: FfmpegCommand) -> Result<(), FfmpegError> {
        let mut args: Vec<String> = vec![
            String::from("-y"), // Force overwrite
            String::from("-i"), // Set input
            format!("assets/input/{}", command.input),
        ];

        if let Some(res) = command.resolution {
            args.push(String::from("-vf"));
            args.push(format!("scale=-2:{}", res as i32));
        }

        if let Some(fps) = command.fps {
            args.push(String::from("-r"));
            args.push(format!("{}", fps as i32));
        }

        args.push(format!("assets/output/{}", command.output));

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

    impl CommandRunner for MockCommandRunner {
        fn run(&mut self, program: &str, args: &[String]) -> io::Result<()> {
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
