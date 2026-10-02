from dataclasses import dataclass
import multiprocessing.shared_memory as sm
import struct
from typing import Optional, Tuple
import torch

HEADER_MAGIC = b"VIDU"
HEADER_VERSION = 1
HEADER_SIZE = 64
HEADER_FORMAT = "=4sIQIIIIIQ20s"

assert struct.calcsize(HEADER_FORMAT) == HEADER_SIZE, f"Header size must be {HEADER_SIZE} bytes"


@dataclass
class ShmHeader:
    """Deterministic 64-byte binary header preceding raw video frames in /dev/shm."""

    magic: bytes = HEADER_MAGIC
    version: int = HEADER_VERSION
    seq_id: int = 0
    width: int = 0
    height: int = 0
    num_frames: int = 0
    channels: int = 3
    pixel_format: int = 0  # 0 = RGB24 Packed, 1 = BGR24, 2 = RGBA32
    data_offset: int = HEADER_SIZE
    reserved: bytes = b"\x00" * 20

    def pack(self) -> bytes:
        return struct.pack(
            HEADER_FORMAT,
            self.magic,
            self.version,
            self.seq_id,
            self.width,
            self.height,
            self.num_frames,
            self.channels,
            self.pixel_format,
            self.data_offset,
            self.reserved,
        )

    @classmethod
    def unpack(cls, buffer: bytes) -> "ShmHeader":
        if len(buffer) < HEADER_SIZE:
            raise ValueError(f"Buffer size {len(buffer)} too small for ShmHeader ({HEADER_SIZE} bytes)")
        fields = struct.unpack(HEADER_FORMAT, buffer[:HEADER_SIZE])
        header = cls(
            magic=fields[0],
            version=fields[1],
            seq_id=fields[2],
            width=fields[3],
            height=fields[4],
            num_frames=fields[5],
            channels=fields[6],
            pixel_format=fields[7],
            data_offset=fields[8],
            reserved=fields[9],
        )
        header.validate()
        return header

    def validate(self) -> None:
        if self.magic != HEADER_MAGIC:
            raise ValueError(f"Invalid SHM header magic: expected {HEADER_MAGIC!r}, got {self.magic!r}")
        if self.version != HEADER_VERSION:
            raise ValueError(f"Unsupported SHM header version: {self.version}")
        if self.data_offset != HEADER_SIZE:
            raise ValueError(f"Invalid data offset: expected {HEADER_SIZE}, got {self.data_offset}")
        if self.width <= 0 or self.height <= 0:
            raise ValueError(f"Invalid frame dimensions: {self.width}x{self.height}")
        if self.num_frames < 0:
            raise ValueError(f"Invalid frame count: {self.num_frames}")
        if self.channels not in (1, 3, 4):
            raise ValueError(f"Invalid channel count: {self.channels}")

    @property
    def payload_bytes(self) -> int:
        return self.num_frames * self.height * self.width * self.channels


class ShmReader:
    """Zero-copy reader for POSIX shared memory segments staged by the host engine."""

    def __init__(self, shm_name: str):
        clean_name = shm_name.lstrip("/")
        self.shm_name = shm_name
        self.clean_name = clean_name
        self.shm = sm.SharedMemory(name=clean_name, create=False)

    def read_header(self) -> ShmHeader:
        raw_header = bytes(self.shm.buf[:HEADER_SIZE])
        return ShmHeader.unpack(raw_header)

    def read_tensor(self) -> Tuple[ShmHeader, torch.Tensor]:
        """Reads frame buffer as a zero-copy PyTorch tensor formatted as [T, C, H, W]."""
        header = self.read_header()
        total_bytes = header.payload_bytes
        offset = header.data_offset

        if len(self.shm.buf) < offset + total_bytes:
            raise ValueError(
                f"Shared memory buffer size ({len(self.shm.buf)}) is smaller than required "
                f"header + payload ({offset + total_bytes})"
            )

        # Zero-copy view into memory mapped region
        raw_buffer = memoryview(self.shm.buf)[offset : offset + total_bytes]
        flat_tensor = torch.frombuffer(raw_buffer, dtype=torch.uint8)

        # Reshape to [T, H, W, C]
        hwc_tensor = flat_tensor.view(header.num_frames, header.height, header.width, header.channels)

        # Permute to [T, C, H, W] without copying memory
        nchw_view = hwc_tensor.permute(0, 3, 1, 2)
        return header, nchw_view

    def close(self) -> None:
        if self.shm is not None:
            try:
                self.shm.close()
            except BufferError:
                pass
            self.shm = None

    def __enter__(self) -> "ShmReader":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()


class ShmWriter:
    """Writer for copying generated frames into POSIX shared memory segments."""

    def __init__(self, shm_name: str):
        clean_name = shm_name.lstrip("/")
        self.shm_name = shm_name
        self.clean_name = clean_name
        self.shm = sm.SharedMemory(name=clean_name, create=False)

    def write_tensor(self, seq_id: int, tensor: torch.Tensor, pixel_format: int = 0) -> ShmHeader:
        """Writes frame tensor into the shared memory segment with a 64-byte header.

        Args:
            seq_id: Monotonically increasing sequence index.
            tensor: Frame tensor formatted as [T, C, H, W] or [T, H, W, C] uint8.
            pixel_format: 0 for Packed RGB24.

        Returns:
            The written ShmHeader.
        """
        # Ensure tensor is CPU uint8
        if tensor.is_cuda:
            tensor = tensor.cpu()
        if tensor.dtype != torch.uint8:
            tensor = tensor.to(torch.uint8)

        if tensor.dim() != 4:
            raise ValueError(f"Expected 4D tensor, got shape {tensor.shape}")

        # Permute [T, C, H, W] -> [T, H, W, C] if needed
        if tensor.shape[1] in (1, 3, 4) and tensor.shape[1] != tensor.shape[3]:
            t, c, h, w = tensor.shape
            hwc_tensor = tensor.permute(0, 2, 3, 1).contiguous()
        else:
            t, h, w, c = tensor.shape
            hwc_tensor = tensor.contiguous()

        header = ShmHeader(
            seq_id=seq_id,
            width=w,
            height=h,
            num_frames=t,
            channels=c,
            pixel_format=pixel_format,
        )

        total_bytes = header.payload_bytes
        offset = header.data_offset

        if len(self.shm.buf) < offset + total_bytes:
            raise ValueError(
                f"Shared memory buffer size ({len(self.shm.buf)}) too small for "
                f"header + payload ({offset + total_bytes})"
            )

        # Write 64-byte header
        self.shm.buf[:HEADER_SIZE] = header.pack()

        # Direct memory copy into mapped buffer
        dest_view = memoryview(self.shm.buf)[offset : offset + total_bytes]
        dest_tensor = torch.frombuffer(dest_view, dtype=torch.uint8).view_as(hwc_tensor)
        dest_tensor.copy_(hwc_tensor)

        return header

    def close(self) -> None:
        if self.shm is not None:
            try:
                self.shm.close()
            except BufferError:
                pass
            self.shm = None

    def __enter__(self) -> "ShmWriter":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()
