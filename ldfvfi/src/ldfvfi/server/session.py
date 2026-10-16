import logging
import time
from typing import Any, Optional, Tuple
import torch

from ldfvfi.proto import MediaMetadata, VfiParameters
from ldfvfi.server.session_runner import SkipConcatSession
from ldfvfi.server.shm import ShmReader, ShmWriter

logger = logging.getLogger(__name__)


class VfiSession:
    """Encapsulates an active interpolation session: runner, input reader, and output writer."""

    def __init__(
        self,
        session_id: str,
        params: VfiParameters,
        media_metadata: MediaMetadata,
        model: Any = None,
        device: str = "cpu",
        dtype: torch.dtype = torch.float32,
        max_input_frames: int = 30,
    ):
        if not session_id:
            raise ValueError("session_id is required")
        if params is None:
            raise ValueError("params is required")
        if media_metadata is None:
            raise ValueError("media_metadata is required")
        if media_metadata.width <= 0 or media_metadata.height <= 0:
            raise ValueError(
                f"media_metadata width ({media_metadata.width}) and height ({media_metadata.height}) must be positive"
            )
        channels = media_metadata.channels if media_metadata.channels > 0 else 3
        if channels not in (1, 3, 4):
            raise ValueError(
                f"media_metadata channels ({channels}) must be 1, 3, or 4"
            )

        self.session_id = session_id
        self.params = params
        self.media_metadata = media_metadata
        self.width = media_metadata.width
        self.height = media_metadata.height
        self.channels = channels
        self.pixel_format = media_metadata.pixel_format
        self.temporal_sf = params.temporal_scale_factor if params.temporal_scale_factor > 0 else 4
        self.max_input_frames = max_input_frames
        self.max_output_frames = max_input_frames * self.temporal_sf

        self.input_shm_name = f"/viduce_{session_id}_in"
        self.output_shm_name = f"/viduce_{session_id}_out"

        input_size = self.max_input_frames * self.width * self.height * self.channels
        output_size = self.max_output_frames * self.width * self.height * self.channels

        self.runner = SkipConcatSession(
            session_id=session_id,
            params=params,
            model=model,
            device=device,
            dtype=dtype,
        )
        self.shm_reader = ShmReader(
            shm_name=self.input_shm_name,
            width=self.width,
            height=self.height,
            channels=self.channels,
            create=True,
            size=input_size,
        )
        self.shm_writer = ShmWriter(
            shm_name=self.output_shm_name,
            width=self.width,
            height=self.height,
            channels=self.channels,
            create=True,
            size=output_size,
        )

    def interpolate_chunk(
        self,
        chunk_index: int,
        num_input_frames: int,
        is_last: bool = False,
    ) -> Tuple[int, float]:
        """Reads input tensor from SHM, runs model interpolation, and writes output tensor to SHM.

        Returns:
            Tuple of (num_output_frames, inference_time_ms).
        """
        input_tensor = self.shm_reader.read_tensor(num_frames=num_input_frames)

        t_start = time.perf_counter()
        output_tensor = self.runner.interpolate_chunk(
            chunk_index=chunk_index,
            input_tensor=input_tensor,
            is_last=is_last,
        )
        elapsed_ms = (time.perf_counter() - t_start) * 1000.0

        num_output_frames = self.shm_writer.write_tensor(tensor=output_tensor)
        return num_output_frames, elapsed_ms

    def close(self) -> None:
        """Closes and unlinks shared memory handles and releases latent caches."""
        self.shm_reader.unlink()
        self.shm_reader.close()
        self.shm_writer.unlink()
        self.shm_writer.close()
        self.runner.cleanup()
