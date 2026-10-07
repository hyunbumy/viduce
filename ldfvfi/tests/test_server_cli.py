import multiprocessing.shared_memory as sm
import os
import signal
import socket
import subprocess
import sys
import time
import unittest
import grpc
import torch

from ldfvfi.proto import (
    ChunkType,
    CreateSessionRequest,
    DestroySessionRequest,
    InterpolateChunkRequest,
    ShmSegmentDescriptor,
    VfiParameters,
)
from ldfvfi.proto.service_pb2_grpc import LdfVfiServiceStub
from ldfvfi.server.shm import HEADER_SIZE, ShmReader, ShmWriter


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class TestServerCLI(unittest.TestCase):

    def test_cli_help(self):
        result = subprocess.run(
            [sys.executable, "-m", "ldfvfi.server.server", "--help"],
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("--port", result.stdout)
        self.assertIn("--mock", result.stdout)
        self.assertIn("--uds", result.stdout)

    def test_daemon_process_spawn_and_shutdown(self):
        port = find_free_port()
        proc = subprocess.Popen(
            [sys.executable, "-m", "ldfvfi.server.server", "--port", str(port), "--mock"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        channel = grpc.insecure_channel(f"127.0.0.1:{port}")
        # Wait up to 5s for gRPC server readiness
        try:
            grpc.channel_ready_future(channel).result(timeout=5.0)
        except grpc.FutureTimeoutError:
            proc.kill()
            stdout, stderr = proc.communicate()
            self.fail(f"Server did not start in time. stderr: {stderr.decode()}")

        stub = LdfVfiServiceStub(channel)

        in_shm_name = f"cli_test_in_{port}"
        out_shm_name = f"cli_test_out_{port}"
        shm_in = sm.SharedMemory(name=in_shm_name, create=True, size=HEADER_SIZE + (15 * 32 * 32 * 3))
        shm_out = sm.SharedMemory(name=out_shm_name, create=True, size=HEADER_SIZE + (60 * 32 * 32 * 3))

        try:
            # 1. CreateSession
            resp = stub.CreateSession(
                CreateSessionRequest(
                    session_id="cli-sess",
                    params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
                    input_shm=ShmSegmentDescriptor(shm_name=in_shm_name),
                    output_shm=ShmSegmentDescriptor(shm_name=out_shm_name),
                ),
                timeout=5.0,
            )
            self.assertTrue(resp.success)

            # 2. Stage frame and interpolate
            writer = ShmWriter(in_shm_name)
            writer.write_tensor(seq_id=0, tensor=torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8))
            writer.close()

            chunk_resp = stub.InterpolateChunk(
                InterpolateChunkRequest(
                    session_id="cli-sess",
                    chunk_index=0,
                    chunk_type=ChunkType.CHUNK_FIRST,
                    num_input_frames=15,
                    input_width=32,
                    input_height=32,
                    is_last_chunk=False,
                ),
                timeout=5.0,
            )
            self.assertTrue(chunk_resp.success)
            self.assertGreater(chunk_resp.num_output_frames, 0)

            # 3. DestroySession
            dest_resp = stub.DestroySession(DestroySessionRequest(session_id="cli-sess"), timeout=5.0)
            self.assertTrue(dest_resp.success)

        finally:
            channel.close()
            shm_in.close()
            shm_in.unlink()
            shm_out.close()
            shm_out.unlink()

            # Terminate daemon via SIGTERM and verify clean exit
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                proc.kill()
                self.fail("Server process did not terminate on SIGTERM within 5s")

            self.assertIn(proc.returncode, (0, -signal.SIGTERM))


if __name__ == "__main__":
    unittest.main()
