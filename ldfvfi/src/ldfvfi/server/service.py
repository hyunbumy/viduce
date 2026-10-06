import logging
import threading
from typing import Optional
import grpc
import torch

from ldfvfi.proto import (
    CreateSessionRequest,
    CreateSessionResponse,
    DestroySessionRequest,
    DestroySessionResponse,
    InterpolateChunkRequest,
    InterpolateChunkResponse,
)
from ldfvfi.proto.service_pb2_grpc import LdfVfiServiceServicer
from ldfvfi.server.model_holder import ModelHolder
from ldfvfi.server.session import VfiSession

logger = logging.getLogger(__name__)


class LdfVfiService(LdfVfiServiceServicer):
    """gRPC servicer for orchestrating LDF-VFI frame interpolation over shared memory."""

    def __init__(self, model_holder: ModelHolder):
        self.model_holder = model_holder
        self._lock = threading.RLock()
        self._active_session: Optional[VfiSession] = None

    def reset(self) -> None:
        """Thread-safe reset of any active session and GPU cache."""
        with self._lock:
            if self._active_session is not None:
                try:
                    self._active_session.close()
                except Exception as e:
                    logger.warning(
                        f"Error closing session '{self._active_session.session_id}': {e}"
                    )
                finally:
                    self._active_session = None
                    self.model_holder.purge_cache()

    def CreateSession(
        self, request: CreateSessionRequest, context: grpc.ServicerContext
    ) -> CreateSessionResponse:
        session_id = request.session_id
        if not session_id:
            return CreateSessionResponse(success=False, error_message="session_id must not be empty")
        if not request.input_shm.shm_name or not request.output_shm.shm_name:
            return CreateSessionResponse(
                success=False,
                error_message="input_shm and output_shm names must not be empty",
            )

        with self._lock:
            # 1. Preempt existing session if active
            if self._active_session is not None:
                logger.info(
                    f"Preempting previous session '{self._active_session.session_id}' "
                    f"with new session '{session_id}'"
                )
                self.reset()

            # 2. Initialize new session
            try:
                self._active_session = VfiSession(
                    session_id=session_id,
                    params=request.params,
                    input_shm_name=request.input_shm.shm_name,
                    output_shm_name=request.output_shm.shm_name,
                    model=self.model_holder.model,
                    device=self.model_holder.device,
                    dtype=self.model_holder.dtype,
                )
                logger.info(f"Session '{session_id}' created successfully")
                return CreateSessionResponse(success=True)
            except Exception as e:
                logger.error(f"Failed to create session '{session_id}': {e}")
                self.reset()
                return CreateSessionResponse(success=False, error_message=f"Failed to create session: {e}")

    def InterpolateChunk(
        self, request: InterpolateChunkRequest, context: grpc.ServicerContext
    ) -> InterpolateChunkResponse:
        session_id = request.session_id
        with self._lock:
            if self._active_session is None or self._active_session.session_id != session_id:
                return InterpolateChunkResponse(
                    success=False,
                    error_message=f"Session '{session_id}' not found",
                    chunk_index=request.chunk_index,
                )

            try:
                num_output_frames, inference_time_ms = self._active_session.interpolate_chunk(
                    chunk_type=request.chunk_type,
                    chunk_index=request.chunk_index,
                    is_last=request.is_last_chunk,
                )
                return InterpolateChunkResponse(
                    success=True,
                    chunk_index=request.chunk_index,
                    num_output_frames=num_output_frames,
                    inference_time_ms=inference_time_ms,
                )
            except torch.cuda.OutOfMemoryError as e:
                logger.error(f"CUDA OOM in session '{session_id}': {e}")
                self.model_holder.purge_cache()
                return InterpolateChunkResponse(
                    success=False,
                    error_message="CUDA out of memory during chunk interpolation",
                    chunk_index=request.chunk_index,
                )
            except Exception as e:
                logger.error(f"Error during chunk interpolation in session '{session_id}': {e}")
                return InterpolateChunkResponse(
                    success=False,
                    error_message=str(e),
                    chunk_index=request.chunk_index,
                )

    def DestroySession(
        self, request: DestroySessionRequest, context: grpc.ServicerContext
    ) -> DestroySessionResponse:
        session_id = request.session_id
        with self._lock:
            if self._active_session is not None and self._active_session.session_id == session_id:
                self.reset()
                logger.info(f"Session '{session_id}' destroyed")
                return DestroySessionResponse(success=True)
            else:
                logger.warning(f"Session '{session_id}' not found for destruction")
                return DestroySessionResponse(
                    success=False,
                    error_message=f"Session '{session_id}' not found",
                )
