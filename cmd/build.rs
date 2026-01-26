fn main() {
    println!("cargo:rustc-link-search=engine/build/lib");
    println!("cargo:rustc-link-search=/lib");
}
