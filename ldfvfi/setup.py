import os
import sys
from setuptools import setup
from setuptools.command.build_py import build_py

try:
    from setuptools.command.editable_wheel import editable_wheel
except ImportError:
    editable_wheel = None


def generate_proto_stubs() -> None:
    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    proto_dir = os.path.abspath(os.path.join(pkg_dir, "..", "proto", "ldfvfi", "v1"))
    proto_file = os.path.join(proto_dir, "service.proto")
    out_dir = os.path.join(pkg_dir, "src", "ldfvfi", "proto")
    os.makedirs(out_dir, exist_ok=True)

    if not os.path.exists(proto_file):
        return

    try:
        from grpc_tools import protoc

        res = protoc.main([
            "protoc",
            f"-I{proto_dir}",
            f"--python_out={out_dir}",
            f"--grpc_python_out={out_dir}",
            proto_file,
        ])
        if res != 0:
            sys.stderr.write(f"Warning: protoc failed with exit code {res}\n")
            return

        grpc_file = os.path.join(out_dir, "service_pb2_grpc.py")
        if os.path.exists(grpc_file):
            with open(grpc_file, "r") as f:
                content = f.read()
            fixed = content.replace(
                "import service_pb2 as service__pb2",
                "from . import service_pb2 as service__pb2",
            )
            with open(grpc_file, "w") as f:
                f.write(fixed)
    except ImportError:
        sys.stderr.write("Warning: grpcio-tools not installed; skipping proto codegen\n")


class CustomBuildPy(build_py):
    def run(self):
        generate_proto_stubs()
        super().run()


cmdclass = {"build_py": CustomBuildPy}

if editable_wheel:
    class CustomEditableWheel(editable_wheel):
        def run(self):
            generate_proto_stubs()
            super().run()

    cmdclass["editable_wheel"] = CustomEditableWheel

setup(cmdclass=cmdclass)
