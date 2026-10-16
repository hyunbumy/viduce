import logging
import threading
from typing import Optional
import uuid
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
        if not request.HasField("media_metadata"):
            context.abort(
                grpc.StatusCode.INVALID_ARGUMENT,
                "media_metadata must be provided in CreateSessionRequest",
            )
        metadata = request.media_metadata
        if metadata.width <= 0 or metadata.height <= 0:
            context.abort(
                grpc.StatusCode.INVALID_ARGUMENT,
                f"width ({metadata.width}) and height ({metadata.height}) must be positive",
            )
        channels = metadata.channels or 3
        if channels not in (1, 3, 4):
            context.abort(
                grpc.StatusCode.INVALID_ARGUMENT,
                f"channels ({channels}) must be 1, 3, or 4",
            )

        with self._lock:
            if self._active_session is not None:
                logger.info(
                    f"Preempting previous session '{self._active_session.session_id}'"
                )
                self.reset()

            session_id = uuid.uuid4().hex[:12]
            try:
                self._active_session = VfiSession(
                    session_id=session_id,
                    params=request.params,
                    media_metadata=metadata,
                    model=self.model_holder.model,
                    device=self.model_holder.device,
                    dtype=self.model_holder.dtype,
                )
                logger.info(f"Session '{session_id}' created successfully")
                return CreateSessionResponse(session_id=session_id)
            except ValueError as e:
                logger.warning(f"Invalid session arguments for '{session_id}': {e}")
                self.reset()
                context.abort(grpc.StatusCode.INVALID_ARGUMENT, str(e))
            except Exception as e:
                logger.error(f"Failed to create session '{session_id}': {e}")
                self.reset()
                context.abort(grpc.StatusCode.INTERNAL, f"Failed to create session: {e}")

    def InterpolateChunk(
        self, request: InterpolateChunkRequest, context: grpc.ServicerContext
    ) -> InterpolateChunkResponse:
        session_id = request.session_id
        with self._lock:
            if self._active_session is None or self._active_session.session_id != session_id:
                context.abort(grpc.StatusCode.NOT_FOUND, f"Session '{session_id}' not found")

            if request.num_input_frames <= 0:
                context.abort(
                    grpc.StatusCode.INVALID_ARGUMENT,
                    f"num_input_frames must be positive, got {request.num_input_frames}",
                )

            try:
                num_output_frames, inference_time_ms = self._active_session.interpolate_chunk(
                    chunk_index=request.chunk_index,
                    num_input_frames=request.num_input_frames,
                    is_last=request.is_last_chunk,
                )
                return InterpolateChunkResponse(
                    chunk_index=request.chunk_index,
                    num_output_frames=num_output_frames,
                    inference_time_ms=inference_time_ms,
                )
            except torch.cuda.OutOfMemoryError as e:
                logger.error(f"CUDA OOM in session '{session_id}': {e}")
                self.model_holder.purge_cache()
                context.abort(grpc.StatusCode.RESOURCE_EXHAUSTED, f"CUDA OOM: {e}")
            except Exception as e:
                logger.error(f"Error during chunk interpolation in session '{session_id}': {e}")
                context.abort(grpc.StatusCode.INTERNAL, f"Inference failed: {e}")

    def DestroySession(
        self, request: DestroySessionRequest, context: grpc.ServicerContext
    ) -> DestroySessionResponse:
        session_id = request.session_id
        with self._lock:
            if self._active_session is None or self._active_session.session_id != session_id:
                context.abort(grpc.StatusCode.NOT_FOUND, f"Session '{session_id}' not found")

            self.reset()
            logger.info(f"Session '{session_id}' destroyed successfully")
            return DestroySessionResponse()
