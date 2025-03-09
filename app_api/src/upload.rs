use axum::{
    http::StatusCode,
    Json
};
use serde::{Deserialize, Serialize};
use serde_with::serde_as;
use serde_with::base64::Base64;

pub async fn upload<'a>(
    Json(payload): Json<UploadRequest>,
) -> (StatusCode, Json<UploadResponse>) {
    // insert your application logic here
    println!("Received data {}", payload.data.len());

    let res = UploadResponse {
        id: 1337,
    };

    (StatusCode::CREATED, Json(res))
}

#[serde_as]
#[derive(Deserialize)]
pub struct UploadRequest {
    #[serde_as(as ="Base64")]
    data: Vec<u8>,
}

#[derive(Serialize)]
pub struct UploadResponse {
    id: u64,
}