import multiprocessing.shared_memory as sm
import unittest
import torch

from ldfvfi.proto import ChunkType, VfiParameters
from ldfvfi.server.session import VfiSession
from ldfvfi.server.shm import HEADER_SIZE, ShmReader, ShmWriter
from ldfvfi.testing.mock_model import create_mock_model


class TestVfiSession(unittest.TestCase):

    def setUp(self):
        self.in_shm_name = "test_vfi_sess_in"
        self.out_shm_name = "test_vfi_sess_out"
        # 15 frames 32x32 RGB
        self.shm_in = sm.SharedMemory(name=self.in_shm_name, create=True, size=HEADER_SIZE + (15 * 32 * 32 * 3))
        # 60 frames 32x32 RGB
        self.shm_out = sm.SharedMemory(name=self.out_shm_name, create=True, size=HEADER_SIZE + (60 * 32 * 32 * 3))

    def tearDown(self):
        for s in (self.shm_in, self.shm_out):
            try:
                s.close()
            except BufferError:
                pass
            try:
                s.unlink()
            except FileNotFoundError:
                pass

    def test_session_roundtrip(self):
        params = VfiParameters(sampling_steps=2, temporal_scale_factor=4)
        model = create_mock_model()
        session = VfiSession(
            session_id="test-sess",
            params=params,
            input_shm_name=self.in_shm_name,
            output_shm_name=self.out_shm_name,
            model=model,
            device="cpu",
            dtype=torch.float32,
        )

        # Stage input frames in SHM
        writer = ShmWriter(self.in_shm_name)
        input_tensor = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)
        writer.write_tensor(seq_id=0, tensor=input_tensor)
        writer.close()

        # Interpolate
        num_out, elapsed_ms = session.interpolate_chunk(
            chunk_type=ChunkType.CHUNK_FIRST,
            chunk_index=0,
            is_last=False,
        )
        self.assertGreater(num_out, 0)
        self.assertGreaterEqual(elapsed_ms, 0.0)

        # Check output SHM
        reader = ShmReader(self.out_shm_name)
        header, out_tensor = reader.read_tensor()
        self.assertEqual(header.seq_id, 0)
        self.assertEqual(out_tensor.shape[0], num_out)
        del out_tensor
        reader.close()

        # Close session
        session.close()
        self.assertIsNone(session.shm_reader.shm)
        self.assertIsNone(session.shm_writer.shm)
        self.assertIsNone(session.runner.x0_prev)
