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
    CreateSessionRequest,
    DestroySessionRequest,
    InterpolateChunkRequest,
    MediaMetadata,
    VfiParameters,
)
from ldfvfi.proto.service_pb2_grpc import LdfVfiServiceStub
from ldfvfi.server.shm import ShmReader, ShmWriter


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
        # Wait up to 10s for gRPC server readiness
        try:
            grpc.channel_ready_future(channel).result(timeout=10.0)
        except grpc.FutureTimeoutError:
            proc.kill()
            stdout, stderr = proc.communicate()
            self.fail(f"Server did not start in time. stderr: {stderr.decode()}")

        stub = LdfVfiServiceStub(channel)

        try:
            # 1. CreateSession (Server generates session_id and allocates SHM segments)
            resp = stub.CreateSession(
                CreateSessionRequest(
                    media_metadata=MediaMetadata(width=32, height=32, channels=3),
                    params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
                ),
                timeout=5.0,
            )
            session_id = resp.session_id
            self.assertTrue(bool(session_id))

            # 2. Stage frame and interpolate
            in_shm_name = f"/viduce_{session_id}_in"
            writer = ShmWriter(in_shm_name, width=32, height=32, channels=3)
            writer.write_tensor(tensor=torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8))
            writer.close()

            chunk_resp = stub.InterpolateChunk(
                InterpolateChunkRequest(
                    session_id=session_id,
                    chunk_index=0,
                    num_input_frames=15,
                    is_last_chunk=False,
                ),
                timeout=5.0,
            )
            self.assertEqual(chunk_resp.chunk_index, 0)
            self.assertGreater(chunk_resp.num_output_frames, 0)

            # 3. DestroySession
            dest_resp = stub.DestroySession(DestroySessionRequest(session_id=session_id), timeout=5.0)
            self.assertIsNotNone(dest_resp)

        finally:
            channel.close()

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
