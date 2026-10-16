import multiprocessing.shared_memory as sm
from typing import Optional
import torch


class ShmReader:
    """Zero-copy reader for POSIX shared memory segments staged by the host engine."""

    def __init__(
        self,
        shm_name: str,
        width: int,
        height: int,
        channels: int = 3,
        create: bool = False,
        size: int = 0,
    ):
        if width <= 0 or height <= 0:
            raise ValueError(f"Invalid frame dimensions: {width}x{height}")
        if channels not in (1, 3, 4):
            raise ValueError(f"Invalid channel count: {channels}")

        clean_name = shm_name.lstrip("/")
        self.shm_name = shm_name
        self.clean_name = clean_name
        self.width = width
        self.height = height
        self.channels = channels
        self.frame_bytes = width * height * channels

        if create:
            try:
                self.shm = sm.SharedMemory(name=clean_name, create=True, size=size)
            except FileExistsError:
                # Existing stale segment: unlink and recreate
                old = sm.SharedMemory(name=clean_name, create=False)
                old.unlink()
                old.close()
                self.shm = sm.SharedMemory(name=clean_name, create=True, size=size)
        else:
            self.shm = sm.SharedMemory(name=clean_name, create=False)

    def read_tensor(self, num_frames: int) -> torch.Tensor:
        """Reads frame buffer as a zero-copy PyTorch tensor formatted as [T, C, H, W]."""
        if num_frames <= 0:
            raise ValueError(f"num_frames must be positive, got {num_frames}")

        total_bytes = num_frames * self.frame_bytes
        if len(self.shm.buf) < total_bytes:
            raise ValueError(
                f"Shared memory buffer size ({len(self.shm.buf)}) is smaller than required "
                f"payload ({total_bytes})"
            )

        # Zero-copy view into memory mapped region starting at offset 0
        raw_buffer = memoryview(self.shm.buf)[:total_bytes]
        flat_tensor = torch.frombuffer(raw_buffer, dtype=torch.uint8)

        # Reshape to [T, H, W, C]
        hwc_tensor = flat_tensor.view(num_frames, self.height, self.width, self.channels)

        # Permute to [T, C, H, W] without copying memory
        nchw_view = hwc_tensor.permute(0, 3, 1, 2)
        return nchw_view

    def close(self) -> None:
        if self.shm is not None:
            try:
                self.shm.close()
            except BufferError:
                pass
            self.shm = None

    def unlink(self) -> None:
        if self.shm is not None:
            try:
                self.shm.unlink()
            except (BufferError, FileNotFoundError):
                pass

    def __enter__(self) -> "ShmReader":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()


class ShmWriter:
    """Writer for copying generated frames into POSIX shared memory segments."""

    def __init__(
        self,
        shm_name: str,
        width: int,
        height: int,
        channels: int = 3,
        create: bool = False,
        size: int = 0,
    ):
        if width <= 0 or height <= 0:
            raise ValueError(f"Invalid frame dimensions: {width}x{height}")
        if channels not in (1, 3, 4):
            raise ValueError(f"Invalid channel count: {channels}")

        clean_name = shm_name.lstrip("/")
        self.shm_name = shm_name
        self.clean_name = clean_name
        self.width = width
        self.height = height
        self.channels = channels
        self.frame_bytes = width * height * channels

        if create:
            try:
                self.shm = sm.SharedMemory(name=clean_name, create=True, size=size)
            except FileExistsError:
                # Existing stale segment: unlink and recreate
                old = sm.SharedMemory(name=clean_name, create=False)
                old.unlink()
                old.close()
                self.shm = sm.SharedMemory(name=clean_name, create=True, size=size)
        else:
            self.shm = sm.SharedMemory(name=clean_name, create=False)

    def write_tensor(self, tensor: torch.Tensor) -> int:
        """Writes frame tensor into the shared memory segment starting at offset 0.

        Args:
            tensor: Frame tensor formatted as [T, C, H, W] or [T, H, W, C] uint8.

        Returns:
            Number of frames written.
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

        if w != self.width or h != self.height or c != self.channels:
            raise ValueError(
                f"Tensor dimensions ({w}x{h}, {c}ch) do not match configured SHM "
                f"({self.width}x{self.height}, {self.channels}ch)"
            )

        total_bytes = t * self.frame_bytes
        if len(self.shm.buf) < total_bytes:
            raise ValueError(
                f"Shared memory buffer size ({len(self.shm.buf)}) too small for "
                f"payload ({total_bytes})"
            )

        # Direct memory copy into mapped buffer starting at byte 0
        dest_view = memoryview(self.shm.buf)[:total_bytes]
        dest_tensor = torch.frombuffer(dest_view, dtype=torch.uint8).view_as(hwc_tensor)
        dest_tensor.copy_(hwc_tensor)

        return t

    def close(self) -> None:
        if self.shm is not None:
            try:
                self.shm.close()
            except BufferError:
                pass
            self.shm = None

    def unlink(self) -> None:
        if self.shm is not None:
            try:
                self.shm.unlink()
            except (BufferError, FileNotFoundError):
                pass

    def __enter__(self) -> "ShmWriter":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()
