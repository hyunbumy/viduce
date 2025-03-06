use cmd::ffmpeg::FfmpegRunner;
use cmd::upscaler::UpscalerRunner;
use std::env;

// TODO(hyunbumy): Figure out how we would package ffmpeg binary
fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        println!("Invalid arguments");
    }

    let option = &args[1];
    match option.as_str() {
        "ffmpeg" => FfmpegRunner::new().run(),
        "upscaler" => UpscalerRunner::new().run(),
        _ => println!("Unknown option {option}")
    }
}
