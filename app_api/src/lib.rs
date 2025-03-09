mod upload;

pub mod api {
    pub mod v1 {
        pub use crate::upload::{*};
    }
}