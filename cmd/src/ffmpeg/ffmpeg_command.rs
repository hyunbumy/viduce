pub enum Resolution {
    R1080P = 1080,
    R720P = 720,
    R480P = 480,
    R240P = 240,
    R144P = 144,
}

pub fn parse_to_resolution(input: &str) -> Option<Resolution> {
    match input.to_uppercase().as_str() {
        "1080P" => Some(Resolution::R1080P),
        "720P" => Some(Resolution::R720P),
        "480P" => Some(Resolution::R480P),
        "240P" => Some(Resolution::R240P),
        "144P" => Some(Resolution::R144P),
        _ => None,
    }
}

pub struct FfmpegCommand {
    pub input: String,
    pub output: String,
    pub resolution: Option<Resolution>,
    pub fps: Option<u32>,
}

impl FfmpegCommand {
    pub fn new(input: &str, output: &str) -> Result<FfmpegCommand, String> {
        if input.is_empty() {
            return Err(String::from("Missing input"));
        }

        if output.is_empty() {
            return Err(String::from("Missing output"));
        }

        Ok(FfmpegCommand {
            input: String::from(input),
            output: String::from(output),
            resolution: None,
            fps: None,
        })
    }

    // Set the target resolution of the output video
    pub fn set_resolution(&mut self, res: Resolution) -> &Self {
        self.resolution = Some(res);
        self
    }

    // set the target fps of the output video
    pub fn set_fps(&mut self, fps: u32) -> &Self {
        self.fps = Some(fps);
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn execute_without_input_fails() {
        let input = "";
        let output = "output.mp4";

        let result = FfmpegCommand::new(input, output);

        assert!(result.is_err_and(|e| e.contains("Missing input")));
    }

    #[test]
    fn execute_without_output_fails() {
        let input = "input.mp4";
        let output = "";

        let result = FfmpegCommand::new(input, output);

        assert!(result.is_err_and(|e| e.contains("Missing output")));
    }
}
