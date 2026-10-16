import multiprocessing.shared_memory as sm
import unittest
import torch
from ldfvfi.server.shm import ShmReader, ShmWriter


class TestShmReaderWriter(unittest.TestCase):

    def setUp(self):
        self.shm_name = "test_viduce_shm_unit"
        self.width = 64
        self.height = 64
        self.channels = 3
        # 5 frames * 64 height * 64 width * 3 channels (no header)
        self.buffer_size = 5 * self.height * self.width * self.channels
        self.raw_shm = sm.SharedMemory(name=self.shm_name, create=True, size=self.buffer_size)

    def tearDown(self):
        try:
            self.raw_shm.close()
        except BufferError:
            pass
        try:
            self.raw_shm.unlink()
        except FileNotFoundError:
            pass

    def test_roundtrip_nchw_tensor(self):
        # Create synthetic video chunk: 5 frames, 3 channels, 64x64
        torch.manual_seed(42)
        input_tensor = torch.randint(0, 256, (5, 3, 64, 64), dtype=torch.uint8)

        # Write via ShmWriter
        writer = ShmWriter(self.shm_name, width=self.width, height=self.height, channels=self.channels)
        num_written = writer.write_tensor(tensor=input_tensor)
        writer.close()
        self.assertEqual(num_written, 5)

        # Read via ShmReader
        reader = ShmReader(self.shm_name, width=self.width, height=self.height, channels=self.channels)
        output_tensor = reader.read_tensor(num_frames=5)

        self.assertEqual(output_tensor.shape, (5, 3, 64, 64))
        self.assertTrue(torch.equal(input_tensor, output_tensor))

        # Explicitly release buffer view before closing reader
        del output_tensor
        reader.close()

    def test_invalid_dimensions_raises(self):
        with self.assertRaises(ValueError):
            ShmReader(self.shm_name, width=0, height=64)
        with self.assertRaises(ValueError):
            ShmReader(self.shm_name, width=64, height=0)
        with self.assertRaises(ValueError):
            ShmReader(self.shm_name, width=64, height=64, channels=2)

    def test_buffer_overflow_raises(self):
        writer = ShmWriter(self.shm_name, width=self.width, height=self.height, channels=self.channels)
        # Attempt to write 10 frames into a 5-frame buffer
        too_big = torch.randint(0, 256, (10, 3, 64, 64), dtype=torch.uint8)
        with self.assertRaises(ValueError):
            writer.write_tensor(too_big)
        writer.close()

    def test_read_zero_or_negative_frames_raises(self):
        reader = ShmReader(self.shm_name, width=self.width, height=self.height, channels=self.channels)
        with self.assertRaises(ValueError):
            reader.read_tensor(num_frames=0)
        with self.assertRaises(ValueError):
            reader.read_tensor(num_frames=-1)
        reader.close()


if __name__ == "__main__":
    unittest.main()
