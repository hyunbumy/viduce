import multiprocessing.shared_memory as sm
import unittest
import torch
from ldfvfi.server.shm import HEADER_SIZE, ShmHeader, ShmReader, ShmWriter


class TestShmHeader(unittest.TestCase):

    def test_pack_and_unpack(self):
        header = ShmHeader(
            seq_id=42,
            width=1920,
            height=1080,
            num_frames=60,
            channels=3,
            pixel_format=0,
        )
        packed = header.pack()
        self.assertEqual(len(packed), HEADER_SIZE)

        unpacked = ShmHeader.unpack(packed)
        self.assertEqual(unpacked.seq_id, 42)
        self.assertEqual(unpacked.width, 1920)
        self.assertEqual(unpacked.height, 1080)
        self.assertEqual(unpacked.num_frames, 60)
        self.assertEqual(unpacked.channels, 3)
        self.assertEqual(unpacked.pixel_format, 0)
        self.assertEqual(unpacked.data_offset, HEADER_SIZE)

    def test_invalid_magic_raises(self):
        bad_buffer = bytearray(HEADER_SIZE)
        bad_buffer[:4] = b"NOPE"
        with self.assertRaises(ValueError) as ctx:
            ShmHeader.unpack(bytes(bad_buffer))
        self.assertIn("Invalid SHM header magic", str(ctx.exception))

    def test_short_buffer_raises(self):
        with self.assertRaises(ValueError):
            ShmHeader.unpack(b"too_short")


class TestShmReaderWriter(unittest.TestCase):

    def setUp(self):
        self.shm_name = "test_viduce_shm_unit"
        # 64-byte header + (5 frames * 64 height * 64 width * 3 channels)
        self.buffer_size = HEADER_SIZE + (5 * 64 * 64 * 3)
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
        writer = ShmWriter(self.shm_name)
        written_header = writer.write_tensor(seq_id=7, tensor=input_tensor)
        writer.close()

        self.assertEqual(written_header.seq_id, 7)
        self.assertEqual(written_header.num_frames, 5)
        self.assertEqual(written_header.channels, 3)
        self.assertEqual(written_header.height, 64)
        self.assertEqual(written_header.width, 64)

        # Read via ShmReader
        reader = ShmReader(self.shm_name)
        read_header, output_tensor = reader.read_tensor()

        self.assertEqual(read_header.seq_id, 7)
        self.assertEqual(output_tensor.shape, (5, 3, 64, 64))
        self.assertTrue(torch.equal(input_tensor, output_tensor))

        # Explicitly release buffer view before closing reader
        del output_tensor
        reader.close()


if __name__ == "__main__":
    unittest.main()
