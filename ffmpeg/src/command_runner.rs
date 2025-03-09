use std::io::Result;

pub trait CommandRunner {
    fn run(&mut self, program: &str, args: &[String]) -> Result<()>;
}
