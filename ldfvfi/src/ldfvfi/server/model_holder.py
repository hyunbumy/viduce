from typing import Any
import torch
from ldfvfi.testing.mock_model import create_mock_model


class ModelHolder:
    """Manages active PyTorch model instance, device placement, and cache purging."""

    def __init__(
        self,
        model: Any,
        device: str = "cuda",
        dtype: torch.dtype = torch.bfloat16,
    ):
        self._model = model
        self._device = device
        self._dtype = dtype

    @property
    def model(self) -> Any:
        return self._model

    @property
    def device(self) -> str:
        return self._device

    @property
    def dtype(self) -> torch.dtype:
        return self._dtype

    def purge_cache(self) -> None:
        """Frees unreferenced cached memory in CUDA."""
        if torch.cuda.is_available():
            torch.cuda.empty_cache()


def create_mock_model_holder() -> ModelHolder:
    return ModelHolder(model=create_mock_model(), device="cpu", dtype=torch.float32)
