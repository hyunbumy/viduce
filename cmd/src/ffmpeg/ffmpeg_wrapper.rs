use crate::command_runner::CommandRunner;
use crate::ffmpeg::FfmpegCommand;
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
pub struct FfmpegWrapper {
    runner: Box<dyn CommandRunner>,
}

impl FfmpegWrapper {
    // Creates a new instance
    pub fn new(runner: Box<dyn CommandRunner>) -> Self {
        FfmpegWrapper { runner }
    }

    pub fn execute(&mut self, command: FfmpegCommand) -> Result<(), FfmpegError> {
        let mut args: Vec<String> = vec![
            String::from("-y"),
            String::from("-i"),
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

    struct MockCommandRunner {
        last_run_command: String,
    }

    impl MockCommandRunner {
        fn new() -> MockCommandRunner {
            MockCommandRunner {
                last_run_command: String::new(),
            }
        }
    }

    impl CommandRunner for MockCommandRunner {
        fn run(&mut self, program: &str, args: &[String]) -> io::Result<()> {
            self.last_run_command = format!("{program} {}", args.join(" "));
            Ok(())
        }
    }

    #[test]
    fn test() {
        // TODO: Implement tests using MockCommandRunner
        let _ = MockCommandRunner::new();
    }
}
