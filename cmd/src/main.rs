use std::io::{self, Write};
use std::process::Command;

// TODO(hyunbumy): Figure out how we would package ffmpeg binary
fn main() {
    // TODO(hyunbumy): Add graceful exit;
    loop {
        println!("Please input the ffmpeg command:");
        let mut input = String::new();
        io::stdin().read_line(&mut input).unwrap();
        let options = input.trim_end();

        let mut cmd = Command::new("ffmpeg");
        if options.len() > 0 {
            cmd.arg(options);
        }
        let output = cmd.output().unwrap();

        // TODO: Log this explicitly instead of stdout
        io::stdout().write_all(&output.stdout).unwrap();
        io::stderr().write_all(&output.stderr).unwrap();

        println!("\n\n");
        println!("=====================");
        println!("Completed with status: {}", output.status);
        println!("=====================");
        println!("\n");
    }
}
