use std::path::PathBuf;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // TODO: Don't hardcode this. Derive it from CARGO_MANIFEST_DIR (or an env
    // var) so it works outside the /usr/src/viduce container layout.
    let lib_dir = "/usr/src/viduce/viduce-engine/build/lib";
    println!("cargo:rustc-link-search=native={lib_dir}");
    // Embed an rpath so the dynamic loader finds libengine_api.so at runtime
    // without needing LD_LIBRARY_PATH.
    println!("cargo:rustc-link-arg=-Wl,-rpath,{lib_dir}");

    // EngineService client stubs, generated from the repo-root proto/ tree
    // shared with the C++ engine. Requires `protoc` on PATH.
    let proto_root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../proto");
    let engine_service_proto = proto_root.join("engine/v1/engine_service.proto");
    tonic_prost_build::configure()
        .build_server(false)
        .compile_protos(&[engine_service_proto], &[proto_root])?;

    Ok(())
}
