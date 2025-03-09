use axum::{
    http::StatusCode,
    Json
};
use serde::{Deserialize, Serialize};

pub async fn upload(
    Json(payload): Json<UploadRequest>,
) -> (StatusCode, Json<UploadResponse>) {
    // insert your application logic here
    println!("Received data {}", payload.data.len());

    let res = UploadResponse {
        id: 1337,
    };

    (StatusCode::CREATED, Json(res))
}

#[derive(Deserialize)]
pub struct UploadRequest {
    data: Vec<u8>,
}

#[derive(Serialize)]
pub struct UploadResponse {
    id: u64,
}