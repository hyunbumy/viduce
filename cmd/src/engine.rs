use libc::{c_char, c_int};

// IMPORTANT: Make sure to set the LD_LIBRARY_PATH correctly to locate the
// dynamic lib.
#[link(name = "engine_api")]
unsafe extern "C" {
    fn DecodeVideo(input_path: *const c_char) -> c_int;
}

pub struct EngineRunner{}

impl EngineRunner {
    pub fn new() -> Self {
        EngineRunner {}
    }

    pub fn run(&mut self) {
        let res = unsafe { DecodeVideo(std::ffi::CString::new("assets/input/input.mp4").unwrap().as_ptr()) };
        println!("DecodeVideo returned: {}", res);
    }
}
