from concurrent import futures
import multiprocessing.shared_memory as sm
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
from ldfvfi.proto.service_pb2_grpc import (
    LdfVfiServiceStub,
    add_LdfVfiServiceServicer_to_server,
)
from ldfvfi.server.model_holder import create_mock_model_holder
from ldfvfi.server.service import LdfVfiService
from ldfvfi.server.shm import HEADER_SIZE, ShmReader, ShmWriter


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

        self.in_shm_name = "test_svc_in_shm"
        self.out_shm_name = "test_svc_out_shm"
        # 15 frames 32x32 RGB
        self.shm_in = sm.SharedMemory(name=self.in_shm_name, create=True, size=HEADER_SIZE + (15 * 32 * 32 * 3))
        # 60 frames 32x32 RGB output
        self.shm_out = sm.SharedMemory(name=self.out_shm_name, create=True, size=HEADER_SIZE + (60 * 32 * 32 * 3))

    def tearDown(self):
        self.channel.close()
        self.server.stop(grace=None)
        for s in (self.shm_in, self.shm_out):
            try:
                s.close()
            except BufferError:
                pass
            try:
                s.unlink()
            except FileNotFoundError:
                pass

    def test_full_rpc_lifecycle(self):
        # 1. CreateSession
        create_req = CreateSessionRequest(
            session_id="rpc-sess-1",
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
            input_shm=ShmSegmentDescriptor(shm_name=self.in_shm_name),
            output_shm=ShmSegmentDescriptor(shm_name=self.out_shm_name),
        )
        resp = self.stub.CreateSession(create_req)
        self.assertTrue(resp.success)

        # 2. Stage input chunk in SHM
        writer = ShmWriter(self.in_shm_name)
        input_tensor = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)
        writer.write_tensor(seq_id=0, tensor=input_tensor)
        writer.close()

        # 3. InterpolateChunk
        chunk_req = InterpolateChunkRequest(
            session_id="rpc-sess-1",
            chunk_index=0,
            chunk_type=ChunkType.CHUNK_FIRST,
            num_input_frames=15,
            input_width=32,
            input_height=32,
            is_last_chunk=False,
        )
        chunk_resp = self.stub.InterpolateChunk(chunk_req)
        self.assertTrue(chunk_resp.success)
        self.assertEqual(chunk_resp.chunk_index, 0)
        self.assertGreater(chunk_resp.num_output_frames, 0)

        # Verify output written to SHM
        reader = ShmReader(self.out_shm_name)
        out_header, out_tensor = reader.read_tensor()
        self.assertEqual(out_header.seq_id, 0)
        self.assertEqual(out_tensor.shape[0], chunk_resp.num_output_frames)
        del out_tensor
        reader.close()

        # 4. DestroySession
        dest_resp = self.stub.DestroySession(DestroySessionRequest(session_id="rpc-sess-1"))
        self.assertTrue(dest_resp.success)

    def test_create_session_preempts_previous_session(self):
        # First session
        create_req = CreateSessionRequest(
            session_id="first-sess",
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
            input_shm=ShmSegmentDescriptor(shm_name=self.in_shm_name),
            output_shm=ShmSegmentDescriptor(shm_name=self.out_shm_name),
        )
        resp1 = self.stub.CreateSession(create_req)
        self.assertTrue(resp1.success)

        # Second create session call preempts first
        create_req2 = CreateSessionRequest(
            session_id="second-sess",
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
            input_shm=ShmSegmentDescriptor(shm_name=self.in_shm_name),
            output_shm=ShmSegmentDescriptor(shm_name=self.out_shm_name),
        )
        resp2 = self.stub.CreateSession(create_req2)
        self.assertTrue(resp2.success)

        # First session is now expired / superseded
        chunk_req = InterpolateChunkRequest(
            session_id="first-sess",
            chunk_index=0,
            chunk_type=ChunkType.CHUNK_FIRST,
            num_input_frames=15,
            input_width=32,
            input_height=32,
            is_last_chunk=False,
        )
        chunk_resp = self.stub.InterpolateChunk(chunk_req)
        self.assertFalse(chunk_resp.success)
        self.assertIn("not found", chunk_resp.error_message.lower())

    def test_interpolate_unknown_session_rejected(self):
        chunk_req = InterpolateChunkRequest(
            session_id="ghost-session",
            chunk_index=0,
            chunk_type=ChunkType.CHUNK_FIRST,
            num_input_frames=15,
            input_width=32,
            input_height=32,
            is_last_chunk=False,
        )
        chunk_resp = self.stub.InterpolateChunk(chunk_req)
        self.assertFalse(chunk_resp.success)
        self.assertIn("not found", chunk_resp.error_message.lower())

    def test_create_session_empty_shm_names_rejected(self):
        req = CreateSessionRequest(
            session_id="empty-shm-sess",
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
            input_shm=ShmSegmentDescriptor(shm_name=""),
            output_shm=ShmSegmentDescriptor(shm_name=self.out_shm_name),
        )
        resp = self.stub.CreateSession(req)
        self.assertFalse(resp.success)
        self.assertIn("must not be empty", resp.error_message.lower())

    def test_servicer_reset(self):
        req = CreateSessionRequest(
            session_id="to-reset-sess",
            params=VfiParameters(sampling_steps=2, temporal_scale_factor=4),
            input_shm=ShmSegmentDescriptor(shm_name=self.in_shm_name),
            output_shm=ShmSegmentDescriptor(shm_name=self.out_shm_name),
        )
        resp = self.stub.CreateSession(req)
        self.assertTrue(resp.success)
        self.assertIsNotNone(self.servicer._active_session)

        # Call reset directly
        self.servicer.reset()
        self.assertIsNone(self.servicer._active_session)
