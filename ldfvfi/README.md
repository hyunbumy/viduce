# LDF-VFI

Local Diffusion Forcing for Video Frame Interpolation (LDF-VFI) inference engine for Viduce.

## Protocol Buffers Codegen & Editor Setup

Protobuf and gRPC stubs (`service_pb2.py` and `service_pb2_grpc.py`) are compiled from the canonical schema at `proto/ldfvfi/v1/service.proto` and are **not checked into version control**.

- **Automatic Build Hook**: Installing or syncing the package (`uv sync` or `pip install -e .`) automatically triggers the build hook in `setup.py` and generates the stubs into `src/ldfvfi/proto/`.
- **Manual Generation**: You can also run `./build_proto.sh` directly.
- **Code Editor Tools (VS Code, PyCharm, Pyright, Pylance)**: On a fresh clone, run `uv sync` (or `./build_proto.sh`) once before opening editor tools so language servers and linters can index the generated stubs for autocompletion and type checking.
