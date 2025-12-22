use crate::upscaler::Upscaler;
use reqwest::blocking::Client;
use reqwest::header::HeaderMap;
use std::collections::HashMap;
use std::error::Error;
use util::uri_io::UriIo;

const API_HOST: &str = "https://api.segmind.com";

const API_ENDPOINT: &str = "/v1/esrgan-video-upscaler";

pub struct SegmindUpscaler<'a> {
    api_host: &'a str,
    api_key: &'a str,
    // We can probably make this a generic, not trait object
    uri_handler: &'a mut dyn UriIo,
}

impl<'a> SegmindUpscaler<'a> {
    pub fn new(api_key: &'a str, uri_handler: &'a mut dyn UriIo) -> Self {
        SegmindUpscaler::new_internal(API_HOST, api_key, uri_handler)
    }

    fn new_internal(api_host: &'a str, api_key: &'a str, uri_handler: &'a mut dyn UriIo) -> Self {
        SegmindUpscaler {
            api_host,
            api_key,
            uri_handler,
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

        self.uri_handler.write(output_uri, &bytes)?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct MockUriHandler {
        bytes: Vec<u8>,
    }

    impl MockUriHandler {
        fn new() -> Self {
            MockUriHandler { bytes: Vec::new() }
        }
    }

    impl UriIo for MockUriHandler {
        fn read(&mut self, _input_uri: &str, _buf: &mut [u8]) -> std::io::Result<usize> {
            Ok(0)
        }

        fn write(&mut self, _output_uri: &str, buf: &[u8]) -> std::io::Result<()> {
            self.bytes = buf.to_vec();
            Ok(())
        }
    }

    #[test]
    fn upscale_success_returns_upscaled_bytes() {
        let mut server = mockito::Server::new();
        let url = server.url();

        let mut mock_uri_handler = MockUriHandler::new();
        let mut upscaler = SegmindUpscaler::new_internal(&url, "test_key", &mut mock_uri_handler);

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
        assert_eq!(mock_uri_handler.bytes, "1234".as_bytes());
    }

    #[test]
    fn upscale_fail_returns_error() {
        let mut server = mockito::Server::new();
        let url = server.url();

        let mut mock_uri_handler = MockUriHandler::new();
        let mut upscaler = SegmindUpscaler::new_internal(&url, "test_key", &mut mock_uri_handler);

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
