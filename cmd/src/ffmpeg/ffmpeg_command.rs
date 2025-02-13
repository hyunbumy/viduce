pub enum Resolution {
    R1080P = 1080,
    R720P = 720,
    R480P = 480,
    R240P = 240,
    R144P = 144,
}

pub struct FfmpegCommand {
    pub input: String,
    pub output: String,
    pub resolution: Option<Resolution>,
    pub fps: Option<u32>,
}

impl FfmpegCommand {
    // TODO: Add validation
    pub fn new(input: &str, output: &str) -> FfmpegCommand {
        FfmpegCommand {
            input: String::from(input),
            output: String::from(output),
            resolution: None,
            fps: None,
        }
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

