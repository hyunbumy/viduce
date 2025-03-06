use std::error::Error;

pub trait Upscaler {
    fn upscale(&mut self, uri: &str) -> Result<Vec<u8>, Box<dyn Error>>;
}
