use std::io::Result;

pub trait ProcessRunner {
    fn run(&mut self, program: &str, args: &[String]) -> Result<()>;
}

mod default_runner;
pub use crate::process_runner::default_runner::DefaultRunner;
