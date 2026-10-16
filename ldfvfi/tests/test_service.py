from concurrent import futures
import unittest
import grpc
import torch

from ldfvfi.proto import (
    CreateSessionRequest,
    CreateSessionResponse,
    DestroySessionRequest,
    DestroySessionResponse,
    InterpolateChunkRequest,
    InterpolateChunkResponse,
    MediaMetadata,
    VfiParameters,
)
from ldfvfi.proto.service_pb2_grpc import (
    LdfVfiServiceStub,
    add_LdfVfiServiceServicer_to_server,
)
from ldfvfi.server.model_holder import create_mock_model_holder
from ldfvfi.server.service import LdfVfiService
from ldfvfi.server.shm import ShmReader, ShmWriter


class TestLdfVfiService(unittest.TestCase):

    def setUp(self):
        self.server = grpc.server(futures.ThreadPoolExecutor(max_workers=2))
        self.model_holder = create_mock_model_holder()
        self.servicer = LdfVfiService(self.model_holder)
        add_LdfVfiServiceServicer_to_server(self.servicer, self.server)
        port = self.server.add_insecure_port("127.0.0.1:0")
        self.server.start()

        self.channel = grpc.insecure_channel(f"127.0.0.1:{port}")
        self.stub = LdfVfiServiceStub(self.channel)

    def tearDown(self):
        self.servicer.reset()
        self.channel.close()
        self.server.stop(grace=None)

    def test_full_rpc_lifecycle(self):
        # 1. CreateSession (Server generates session_id and creates SHM segments)
        create_req = CreateSessionRequest(
            media_metadata=MediaMetadata(width=32, height=32, channels=3),
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
        )
        resp = self.stub.CreateSession(create_req)
        session_id = resp.session_id
        self.assertTrue(bool(session_id))

        in_shm_name = f"/viduce_{session_id}_in"
        out_shm_name = f"/viduce_{session_id}_out"

        # 2. Stage input chunk in SHM
        writer = ShmWriter(in_shm_name, width=32, height=32, channels=3)
        input_tensor = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)
        writer.write_tensor(tensor=input_tensor)
        writer.close()

        # 3. InterpolateChunk
        chunk_req = InterpolateChunkRequest(
            session_id=session_id,
            chunk_index=0,
            num_input_frames=15,
            is_last_chunk=False,
        )
        chunk_resp = self.stub.InterpolateChunk(chunk_req)
        self.assertEqual(chunk_resp.chunk_index, 0)
        self.assertGreater(chunk_resp.num_output_frames, 0)

        # Verify output written to SHM
        reader = ShmReader(out_shm_name, width=32, height=32, channels=3)
        out_tensor = reader.read_tensor(num_frames=chunk_resp.num_output_frames)
        self.assertEqual(out_tensor.shape[0], chunk_resp.num_output_frames)
        del out_tensor
        reader.close()

        # 4. DestroySession
        dest_resp = self.stub.DestroySession(DestroySessionRequest(session_id=session_id))
        self.assertIsInstance(dest_resp, DestroySessionResponse)

    def test_create_session_preempts_previous_session(self):
        # First session
        create_req = CreateSessionRequest(
            media_metadata=MediaMetadata(width=32, height=32, channels=3),
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
        )
        resp1 = self.stub.CreateSession(create_req)
        first_session_id = resp1.session_id

        # Second create session call preempts first
        resp2 = self.stub.CreateSession(create_req)
        second_session_id = resp2.session_id
        self.assertNotEqual(first_session_id, second_session_id)

        # First session is now expired / superseded -> NOT_FOUND gRPC error
        chunk_req = InterpolateChunkRequest(
            session_id=first_session_id,
            chunk_index=0,
            num_input_frames=15,
            is_last_chunk=False,
        )
        with self.assertRaises(grpc.RpcError) as ctx:
            self.stub.InterpolateChunk(chunk_req)
        self.assertEqual(ctx.exception.code(), grpc.StatusCode.NOT_FOUND)

    def test_interpolate_unknown_session_rejected(self):
        chunk_req = InterpolateChunkRequest(
            session_id="ghost-session",
            chunk_index=0,
            num_input_frames=15,
            is_last_chunk=False,
        )
        with self.assertRaises(grpc.RpcError) as ctx:
            self.stub.InterpolateChunk(chunk_req)
        self.assertEqual(ctx.exception.code(), grpc.StatusCode.NOT_FOUND)

    def test_create_session_missing_metadata_rejected(self):
        req = CreateSessionRequest(
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
        )
        with self.assertRaises(grpc.RpcError) as ctx:
            self.stub.CreateSession(req)
        self.assertEqual(ctx.exception.code(), grpc.StatusCode.INVALID_ARGUMENT)

    def test_create_session_invalid_dimensions_rejected(self):
        req = CreateSessionRequest(
            media_metadata=MediaMetadata(width=0, height=32, channels=3),
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
        )
        with self.assertRaises(grpc.RpcError) as ctx:
            self.stub.CreateSession(req)
        self.assertEqual(ctx.exception.code(), grpc.StatusCode.INVALID_ARGUMENT)

    def test_interpolate_zero_frames_rejected(self):
        create_req = CreateSessionRequest(
            media_metadata=MediaMetadata(width=32, height=32, channels=3),
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
        )
        resp = self.stub.CreateSession(create_req)
        session_id = resp.session_id

        chunk_req = InterpolateChunkRequest(
            session_id=session_id,
            chunk_index=0,
            num_input_frames=0,
            is_last_chunk=False,
        )
        with self.assertRaises(grpc.RpcError) as ctx:
            self.stub.InterpolateChunk(chunk_req)
        self.assertEqual(ctx.exception.code(), grpc.StatusCode.INVALID_ARGUMENT)


if __name__ == "__main__":
    unittest.main()
