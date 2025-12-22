use std::fs::File;
use std::io::{self, Read, Write};

// Read bytes from or write bytes to the given URI
pub trait UriIo {
    fn read(&mut self, input_uri: &str, buf: &mut [u8]) -> io::Result<usize>;
    fn write(&mut self, output_uri: &str, buf: &[u8]) -> io::Result<()>;
}

pub struct UriHandler {}

impl UriHandler {
    pub fn new() -> Self {
        UriHandler {}
    }
}

impl UriIo for UriHandler {
    fn read(&mut self, input_uri: &str, buf: &mut [u8]) -> io::Result<usize> {
        // Handle read based on the URI type
        // For now, assume everything is a file.
        let mut file = File::open(input_uri)?;

        file.read(buf)
    }

    fn write(&mut self, output_uri: &str, buf: &[u8]) -> io::Result<()> {
        // Handle write based on the URI type
        // For now, assume everything is a file.
        let mut file = File::create(output_uri)?;

        file.write_all(buf)?;
        file.flush()?;

        Ok(())
    }
}
