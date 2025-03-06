use crate::upscaler::Upscaler;
use reqwest::blocking::Client;
use reqwest::header::HeaderMap;
use std::collections::HashMap;
use std::error::Error;

const API_ENDPOINT: &str = "/v1/esrgan-video-upscaler";

pub struct SegmindUpscaler<'a> {
    api_host: &'a str,
    api_key: &'a str,
}

impl<'a> SegmindUpscaler<'a> {
    pub fn new(api_host: &'a str, api_key: &'a str) -> Self {
        SegmindUpscaler { api_host, api_key }
    }
}

impl<'a> Upscaler for SegmindUpscaler<'a> {
    fn upscale(&mut self, uri: &str) -> Result<Vec<u8>, Box<dyn Error>> {
        let client = Client::new();

        let mut headers = HeaderMap::new();
        headers.insert("x-api-key", self.api_key.parse().unwrap());

        let body = HashMap::from([
            ("crop_to_fit", "False"),
            ("input_video", uri),
            ("res_model", "RealESRGAN_x4plus"),
            ("resolution", "FHD"),
        ]);

        let res = client
            .post(format!("{}{}", self.api_host, API_ENDPOINT))
            .headers(headers)
            .json(&body)
            .send()?
            .error_for_status()?;

        Ok(res.bytes().map(|bytes| bytes.to_vec())?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn upscale_success_returns_upscaled_bytes() {
        let mut server = mockito::Server::new();
        let url = server.url();
        let mut upscaler = SegmindUpscaler::new(&url, "test_key");

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

        let result = upscaler.upscale("input_video.mp4");

        mock.assert();
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "1234".as_bytes());
    }

    #[test]
    fn upscale_fail_returns_error() {
        let mut server = mockito::Server::new();
        let url = server.url();
        let mut upscaler = SegmindUpscaler::new(&url, "test_key");

        let mock = server
            .mock("POST", "/v1/esrgan-video-upscaler")
            .with_status(400)
            .create();

        let result = upscaler.upscale("input_video.mp4");

        mock.assert();
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("400 Bad Request"));
    }
}
