import unittest
import torch

from ldfvfi.proto import MediaMetadata, VfiParameters
from ldfvfi.server.session import VfiSession
from ldfvfi.server.shm import ShmReader, ShmWriter
from ldfvfi.testing.mock_model import create_mock_model


class TestVfiSession(unittest.TestCase):

    def test_session_roundtrip(self):
        params = VfiParameters(sampling_steps=2, temporal_scale_factor=4)
        metadata = MediaMetadata(width=32, height=32, channels=3)
        model = create_mock_model()
        session = VfiSession(
            session_id="test-sess",
            params=params,
            media_metadata=metadata,
            model=model,
            device="cpu",
            dtype=torch.float32,
        )

        try:
            # Stage input frames in SHM
            writer = ShmWriter(session.input_shm_name, width=32, height=32, channels=3)
            input_tensor = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)
            writer.write_tensor(tensor=input_tensor)
            writer.close()

            # Interpolate chunk 0
            num_out, elapsed_ms = session.interpolate_chunk(
                chunk_index=0,
                num_input_frames=15,
                is_last=False,
            )
            self.assertGreater(num_out, 0)
            self.assertGreaterEqual(elapsed_ms, 0.0)

            # Check output SHM
            reader = ShmReader(session.output_shm_name, width=32, height=32, channels=3)
            out_tensor = reader.read_tensor(num_frames=num_out)
            self.assertEqual(out_tensor.shape[0], num_out)
            del out_tensor
            reader.close()
        finally:
            # Close session (unlinks SHM handles)
            session.close()

        self.assertIsNone(session.shm_reader.shm)
        self.assertIsNone(session.shm_writer.shm)
        self.assertIsNone(session.runner.x0_prev)

    def test_session_validation(self):
        params = VfiParameters(sampling_steps=2, temporal_scale_factor=4)
        metadata = MediaMetadata(width=32, height=32, channels=3)

        # Missing media_metadata
        with self.assertRaises(ValueError):
            VfiSession(session_id="s1", params=params, media_metadata=None)

        # Invalid width
        with self.assertRaises(ValueError):
            VfiSession(
                session_id="s1",
                params=params,
                media_metadata=MediaMetadata(width=0, height=32, channels=3),
            )

        # Invalid height
        with self.assertRaises(ValueError):
            VfiSession(
                session_id="s1",
                params=params,
                media_metadata=MediaMetadata(width=32, height=-1, channels=3),
            )

        # Invalid channels
        with self.assertRaises(ValueError):
            VfiSession(
                session_id="s1",
                params=params,
                media_metadata=MediaMetadata(width=32, height=32, channels=2),
            )

        # Missing params
        with self.assertRaises(ValueError):
            VfiSession(session_id="s1", params=None, media_metadata=metadata)

        # Missing session_id
        with self.assertRaises(ValueError):
            VfiSession(session_id="", params=params, media_metadata=metadata)


if __name__ == "__main__":
    unittest.main()
