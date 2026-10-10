fn main() {
    // TODO: Don't hardcode this. Derive it from CARGO_MANIFEST_DIR (or an env
    // var) so it works outside the /usr/src/viduce container layout.
    let lib_dir = "/usr/src/viduce/viduce-engine/build/lib";
    println!("cargo:rustc-link-search=native={lib_dir}");
    // Embed an rpath so the dynamic loader finds libengine_api.so at runtime
    // without needing LD_LIBRARY_PATH.
    println!("cargo:rustc-link-arg=-Wl,-rpath,{lib_dir}");
}
