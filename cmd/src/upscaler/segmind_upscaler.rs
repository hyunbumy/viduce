use crate::upscaler::Upscaler;
use reqwest::blocking::Client;
use reqwest::{header::HeaderMap, StatusCode};
use std::collections::HashMap;

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
    fn upscale(&mut self, uri: &str) -> Result<Vec<u8>, String> {
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
            .post(self.api_host)
            .headers(headers)
            .json(&body)
            .send()
            .map_err(|error| error.to_string())?;

        if res.status() != StatusCode::OK {
            return Err(format!("Request error {}", res.status()));
        }

        res.bytes()
            .map(|bytes| bytes.to_vec())
            .map_err(|error| error.to_string())
    }
}
