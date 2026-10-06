import time
from typing import Any, Tuple
import torch

from ldfvfi.proto import ChunkType, VfiParameters
from ldfvfi.server.session_runner import SkipConcatSession
from ldfvfi.server.shm import ShmReader, ShmWriter


class VfiSession:
    """Encapsulates an active interpolation session: runner, input reader, and output writer."""

    def __init__(
        self,
        session_id: str,
        params: VfiParameters,
        input_shm_name: str,
        output_shm_name: str,
        model: Any,
        device: str,
        dtype: torch.dtype,
    ):
        self.session_id = session_id
        self.runner = SkipConcatSession(
            session_id=session_id,
            params=params,
            model=model,
            device=device,
            dtype=dtype,
        )
        self.shm_reader = ShmReader(input_shm_name)
        self.shm_writer = ShmWriter(output_shm_name)

    def interpolate_chunk(
        self,
        chunk_type: ChunkType,
        chunk_index: int,
        is_last: bool = False,
    ) -> Tuple[int, float]:
        """Reads input tensor from SHM, runs model interpolation, and writes output tensor to SHM.

        Returns:
            Tuple of (num_output_frames, inference_time_ms).
        """
        in_header, input_tensor = self.shm_reader.read_tensor()

        t_start = time.perf_counter()
        output_tensor = self.runner.interpolate_chunk(
            chunk_type=chunk_type,
            input_tensor=input_tensor,
            is_last=is_last,
        )
        elapsed_ms = (time.perf_counter() - t_start) * 1000.0

        self.shm_writer.write_tensor(seq_id=chunk_index, tensor=output_tensor)
        return output_tensor.shape[0], elapsed_ms

    def close(self) -> None:
        """Closes shared memory handles and releases latent caches."""
        self.shm_reader.close()
        self.shm_writer.close()
        self.runner.cleanup()
