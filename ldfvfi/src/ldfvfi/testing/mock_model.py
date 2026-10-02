import torch
from ldfvfi.models.precond import SpatialTiledEncoder3D


class MockVAE(SpatialTiledEncoder3D):
    """Mock VAE for fast CPU unit testing."""

    def __init__(self, t_tile: int = 20):
        self.t_tile = t_tile

    def decode(self, xt: torch.Tensor, **kwargs) -> torch.Tensor:
        # xt shape: [1, nt, c, t, h_lat, w_lat]
        b, nt, c, t, h_lat, w_lat = xt.shape
        t_total = nt * self.t_tile
        # 8x spatial upsampling like Wan VAE
        return torch.zeros(
            (b, 3, t_total, h_lat * 8, w_lat * 8),
            dtype=torch.float32,
            device=xt.device,
        )


class MockEncoder:
    """Mock conditioning encoder for fast CPU unit testing."""

    def __init__(self, channels: int = 16, t_tile: int = 5, h_lat: int = 4, w_lat: int = 4):
        self.channels = channels
        self.t_tile = t_tile
        self.h_lat = h_lat
        self.w_lat = w_lat

    def encode(self, x: torch.Tensor, for_train: bool = False) -> torch.Tensor:
        b = x.shape[0]
        nt = 3  # 60 // 20
        nh = 1
        nw = 1
        return torch.zeros(
            (b, nt, nh, nw, self.channels, self.t_tile, self.h_lat, self.w_lat),
            dtype=x.dtype,
            device=x.device,
        )


class MockModel:
    """Mock LDF-VFI preconditioned model for unit testing."""

    def __init__(self, channels: int = 16, t_tile: int = 5):
        self.lq_encoder = MockEncoder(channels=channels, t_tile=t_tile)
        self.msk_encoder = MockEncoder(channels=channels, t_tile=t_tile)
        self.vae = MockVAE(t_tile=20)

    def predict_v(
        self, xt: torch.Tensor, t: torch.Tensor, y: torch.Tensor, m: torch.Tensor
    ) -> torch.Tensor:
        return torch.zeros_like(xt)


def create_mock_model(device: str = "cpu", dtype: torch.dtype = torch.float32) -> MockModel:
    return MockModel()
