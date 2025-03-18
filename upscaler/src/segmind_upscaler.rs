use crate::upscaler::Upscaler;
use reqwest::blocking::Client;
use reqwest::header::HeaderMap;
use std::collections::HashMap;
use std::error::Error;
use std::fs::File;
use std::io::prelude::*;

const API_HOST: &str = "https://api.segmind.com";

const API_ENDPOINT: &str = "/v1/esrgan-video-upscaler";

type WriterFactory = fn(&str) -> Result<Box<dyn Write>, Box<dyn Error>>;

pub struct SegmindUpscaler<'a> {
    api_host: &'a str,
    api_key: &'a str,
    writer_factory: WriterFactory,
}

impl<'a> SegmindUpscaler<'a> {
    pub fn new(api_key: &'a str) -> Self {
        SegmindUpscaler::new_internal(API_HOST, api_key, |output_uri| {
            File::create(output_uri)
                .map(|file| Box::new(file) as Box<dyn Write>)
                .map_err(|e| Box::new(e) as Box<dyn Error>)
        })
    }

    fn new_internal(api_host: &'a str, api_key: &'a str, writer_factory: WriterFactory) -> Self {
        SegmindUpscaler {
            api_host,
            api_key,
            writer_factory,
        }
    }
}

impl<'a> Upscaler for SegmindUpscaler<'a> {
    fn upscale(&mut self, input_uri: &str, output_uri: &str) -> Result<(), Box<dyn Error>> {
        let client = Client::new();

        let mut headers = HeaderMap::new();
        headers.insert("x-api-key", self.api_key.parse().unwrap());

        let body = HashMap::from([
            ("crop_to_fit", "False"),
            ("input_video", input_uri),
            ("res_model", "RealESRGAN_x4plus"),
            ("resolution", "FHD"),
        ]);

        let res = client
            .post(format!("{}{}", self.api_host, API_ENDPOINT))
            .headers(headers)
            .json(&body)
            .send()?
            .error_for_status()?;
        let bytes = res.bytes()?;

        let mut output_writer = (self.writer_factory)(output_uri)?;
        output_writer.write_all(bytes.iter().as_slice())?;
        output_writer.flush()?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct MockWriter {
        expected_bytes: &'static [u8],
        bytes: Vec<u8>,
    }

    impl MockWriter {
        fn new(expected_bytes: &'static [u8]) -> Self {
            MockWriter {
                expected_bytes,
                bytes: Vec::new(),
            }
        }
    }

    impl Write for MockWriter {
        fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
            self.bytes.extend_from_slice(buf);
            Ok(buf.len())
        }

        fn flush(&mut self) -> std::io::Result<()> {
            assert_eq!(self.bytes, self.expected_bytes.to_vec());
            Ok(())
        }
    }

    #[test]
    fn upscale_success_returns_upscaled_bytes() {
        let mut server = mockito::Server::new();
        let url = server.url();
        let mut upscaler = SegmindUpscaler::new_internal(&url, "test_key", |output_uri| {
            assert_eq!(output_uri, "output.mp4");
            Ok(Box::new(MockWriter::new(b"1234")))
        });

        let mock = server
            .mock("POST", "/v1/esrgan-video-upscaler")
            .match_header("x-api-key", "test_key")
            .match_body(mockito::Matcher::PartialJsonString(
                r#"{
                  "crop_to_fit": "False",
                  "input_video": "input_video.mp4",
                  "res_model": "RealESRGAN_x4plus",
                  "resolution": "FHD"
                }"#
                .to_string(),
            ))
            .with_status(200)
            .with_body("1234")
            .create();

        let result = upscaler.upscale("input_video.mp4", "output.mp4");

        mock.assert();
        assert!(result.is_ok());
    }

    #[test]
    fn upscale_fail_returns_error() {
        let mut server = mockito::Server::new();
        let url = server.url();
        let mut upscaler = SegmindUpscaler::new_internal(&url, "test_key", |_output_uri| {
            Ok(Box::new(MockWriter::new(b"")))
        });

        let mock = server
            .mock("POST", "/v1/esrgan-video-upscaler")
            .with_status(400)
            .create();

        let result = upscaler.upscale("input_video.mp4", "output.mp4");

        mock.assert();
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("400 Bad Request"));
    }
}
