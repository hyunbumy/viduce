use std::error::Error;

// Upscaler is a misnomer; it should really be an "Enhancer".
pub trait Upscaler {
    // Enhances a given video and writes the results in the output URI.
    fn upscale(&mut self, input_uri: &str, output_url: &str) -> Result<(), Box<dyn Error>>;
}
