use std::io::Result;

pub trait ProcessRunner {
<<<<<<< HEAD
    fn run(&mut self, program: &str, args: &[&str]) -> Result<()>;
=======
    fn run(&mut self, program: &str, args: &[String]) -> Result<()>;
>>>>>>> 477e331 (Move ProcessRunner into a separate crate.)
}

mod default_runner;
pub use crate::process_runner::default_runner::DefaultRunner;
