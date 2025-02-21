mod segmind_upscaler;

trait Upscaler {
    fn upscale(&mut self, uri: &str) -> Result<Vec<u8>, String>;
}
