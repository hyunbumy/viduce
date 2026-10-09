#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p src/ldfvfi/proto

uv run python -m grpc_tools.protoc \
    -I../proto/ldfvfi/v1 \
    --python_out=src/ldfvfi/proto \
    --grpc_python_out=src/ldfvfi/proto \
    ../proto/ldfvfi/v1/service.proto

# Fix relative import in generated gRPC stub for package usage
if [[ "$OSTYPE" == "darwin"* ]]; then
    sed -i '' -E 's/^import service_pb2 as/from . import service_pb2 as/' src/ldfvfi/proto/service_pb2_grpc.py
else
    sed -i -E 's/^import service_pb2 as/from . import service_pb2 as/' src/ldfvfi/proto/service_pb2_grpc.py
fi

echo "Successfully generated Python proto stubs in src/ldfvfi/proto/"
