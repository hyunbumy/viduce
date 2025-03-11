use crate::process_runner::ProcessRunner;
use std::{io, io::Write, process::Command};

pub struct DefaultRunner {}

impl DefaultRunner {
    pub fn new() -> DefaultRunner {
        DefaultRunner {}
    }
}

impl ProcessRunner for DefaultRunner {
    fn run(&mut self, program: &str, args: &[String]) -> io::Result<()> {
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
