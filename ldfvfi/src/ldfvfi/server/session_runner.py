from typing import Optional, Tuple
from einops import rearrange, repeat
import numpy as np
import torch
import torch.nn.functional as F

from ldfvfi.models.precond import Precond, SpatialTiledConditionEncoder3D, SpatialTiledEncoder3D
from ldfvfi.ops.temporal import upsample_temporal
from ldfvfi.proto import ChunkType, VfiParameters


def vae_decode(vae, xt: torch.Tensor, **kwargs) -> torch.Tensor:
    """Tiled VAE decoding helper matching Wan VAE specifications."""
    if isinstance(vae, SpatialTiledEncoder3D):
        return vae.decode(xt)
    elif isinstance(vae, SpatialTiledConditionEncoder3D):
        lq = kwargs.get("lq")
        msk = kwargs.get("msk")
        decode_h = xt.shape[-2] * vae.spatial_compression_ratio
        decode_w = xt.shape[-1] * vae.spatial_compression_ratio
        lq = F.pad(lq, (0, decode_w - lq.shape[-1], 0, decode_h - lq.shape[-2]))
        lq = rearrange(lq, "b c (nt t) h w -> b nt c t h w", nt=xt.shape[1])
        msk = msk[..., 0, 0]
        msk = repeat(msk, "b c t -> b c t h w", h=decode_h, w=decode_w)
        msk = rearrange(msk, "b c (nt t) h w -> b nt c t h w", nt=xt.shape[1])
        return vae.decode(xt, lq, msk)
    else:
        raise RuntimeError(f"Unsupported VAE type: {type(vae)}")


def compute_schedule(num_steps: int, t_shift: float = 1.0) -> np.ndarray:
    """Compute shifted linear diffusion time schedule."""
    ts = torch.linspace(1.0, 0.0, steps=num_steps + 1).numpy()
    return t_shift * ts / (1.0 + (t_shift - 1.0) * ts)


class SkipConcatSession:
    """Stateful diffusion forcing session preserving boundary latents across chunk RPCs."""

    def __init__(
        self,
        session_id: str,
        params: VfiParameters,
        model: Optional[Precond] = None,
        device: str = "cpu",
        dtype: torch.dtype = torch.bfloat16,
    ):
        self.session_id = session_id
        self.params = params
        self.model = model
        self.device = device
        self.dtype = dtype

        # Diffusion schedule
        self.num_steps = params.sampling_steps if params.sampling_steps > 0 else 10
        self.t_shift = params.t_shift if params.t_shift > 0 else 1.0
        self.t_cond = params.t_cond if params.t_cond > 0 else 0.1
        self.temporal_sf = params.temporal_scale_factor if params.temporal_scale_factor > 0 else 4
        self.train_num_frames = params.train_num_frames if params.train_num_frames > 0 else 60
        self.tile_min_t = params.tile_min_t if params.tile_min_t > 0 else 20
        self.nT_cond = 1

        self.ts = compute_schedule(self.num_steps, self.t_shift)

        # Boundary latent state maintained across calls in VRAM
        self.x0_prev: Optional[torch.Tensor] = None
        self.x0_skip: Optional[torch.Tensor] = None
        self.x0_prev_: Optional[torch.Tensor] = None

    def interpolate_chunk(
        self,
        chunk_type: ChunkType,
        input_tensor: torch.Tensor,
        is_last: bool = False,
    ) -> torch.Tensor:
        """Interpolates intermediate motion frames for a chunk staged in memory.

        Args:
            chunk_type: Chunk role tag (CHUNK_FIRST, CHUNK_SKIP, CHUNK_CONCAT, CHUNK_LAST, CHUNK_STANDALONE).
            input_tensor: Input frames of shape [T_in, 3, H, W] uint8.
            is_last: Whether this is the final video chunk.

        Returns:
            Generated frames of shape [T_out, 3, H, W] uint8.
        """
        if chunk_type == ChunkType.CHUNK_FIRST:
            return self._interpolate_first(input_tensor)
        elif chunk_type == ChunkType.CHUNK_SKIP:
            return self._interpolate_skip(input_tensor)
        elif chunk_type == ChunkType.CHUNK_CONCAT:
            return self._interpolate_concat(input_tensor)
        elif chunk_type == ChunkType.CHUNK_LAST:
            return self._interpolate_last(input_tensor)
        elif chunk_type == ChunkType.CHUNK_STANDALONE:
            return self._interpolate_standalone(input_tensor)
        else:
            raise ValueError(f"Unknown ChunkType: {chunk_type}")

    def _prepare_lq_and_msk(
        self,
        input_tensor: torch.Tensor,
        num_frames: int,
    ) -> Tuple[torch.Tensor, torch.Tensor, int, int]:
        """Upsamples and normalizes input frames and constructs conditioning mask."""
        _, _, h, w = input_tensor.shape
        t_out = len(input_tensor) * self.temporal_sf
        msk = torch.zeros(t_out, dtype=torch.bool)
        msk[:: self.temporal_sf] = True

        msk = F.pad(msk, (0, max(0, num_frames - len(msk))))[:num_frames]
        lq = upsample_temporal(input_tensor, msk, mode="nearest")

        lq = (
            rearrange(lq, "t c h w -> 1 c t h w")
            .to(device=self.device, dtype=self.dtype)
            .div(127.5)
            .sub(1.0)
        )
        msk_tensor = repeat(msk, "t -> 1 1 t h w", h=lq.shape[-2], w=lq.shape[-1]).to(
            device=self.device, dtype=self.dtype
        )
        return lq, msk_tensor, h, w

    def _denoise_loop(
        self,
        xt: torch.Tensor,
        y: torch.Tensor,
        m: torch.Tensor,
    ) -> torch.Tensor:
        """Standard flow-matching Euler denoising trajectory."""
        for i in range(self.num_steps):
            t = (
                torch.tensor([self.ts[i]], device=self.device, dtype=self.dtype)
                .expand(xt.shape[:-4])
                .contiguous()
            )
            vt = self.model.predict_v(xt, t, y, m)
            xt = xt + vt * (self.ts[i + 1] - self.ts[i])
        return xt

    def _interpolate_first(self, input_tensor: torch.Tensor) -> torch.Tensor:
        nT = self.train_num_frames // self.tile_min_t
        lq, msk, h, w = self._prepare_lq_and_msk(input_tensor, self.train_num_frames)

        y = self.model.lq_encoder.encode(lq, for_train=True)
        m = self.model.msk_encoder.encode(msk, for_train=True)
        xt = torch.randn_like(y)

        xt = self._denoise_loop(xt, y, m)
        xt = xt[:, : nT - self.nT_cond]

        # Cache tail boundary latent for subsequent infilling
        self.x0_prev = xt[:, xt.shape[1] - self.nT_cond :]

        # VAE decode
        xt = rearrange(xt, "1 nt nh nw c t h w -> 1 nt c t (nh h) (nw w)")
        lq_sub = lq[:, :, : (nT - self.nT_cond) * self.tile_min_t]
        msk_sub = msk[:, :, : (nT - self.nT_cond) * self.tile_min_t]

        pred = vae_decode(self.model.vae, xt, lq=lq_sub, msk=msk_sub)[..., :h, :w]
        return rearrange(pred, "1 c t h w -> t c h w").add(1.0).mul(127.5).clip(0, 255).byte()

    def _interpolate_skip(self, input_tensor: torch.Tensor) -> torch.Tensor:
        nT = self.train_num_frames // self.tile_min_t
        lq, msk, h, w = self._prepare_lq_and_msk(input_tensor, self.train_num_frames)

        y = self.model.lq_encoder.encode(lq, for_train=True)
        m = self.model.msk_encoder.encode(msk, for_train=True)
        xt = torch.randn_like(y)

        xt = self._denoise_loop(xt, y, m)
        xt = xt[:, self.nT_cond : nT - self.nT_cond]

        # Cache forward skip boundary latents
        self.x0_skip = xt[:, : self.nT_cond]
        self.x0_prev_ = xt[:, xt.shape[1] - self.nT_cond :]

        # VAE decode
        xt = rearrange(xt, "1 nt nh nw c t h w -> 1 nt c t (nh h) (nw w)")
        lq_sub = lq[:, :, self.nT_cond * self.tile_min_t : (nT - self.nT_cond) * self.tile_min_t]
        msk_sub = msk[:, :, self.nT_cond * self.tile_min_t : (nT - self.nT_cond) * self.tile_min_t]

        pred = vae_decode(self.model.vae, xt, lq=lq_sub, msk=msk_sub)[..., :h, :w]
        return rearrange(pred, "1 c t h w -> t c h w").add(1.0).mul(127.5).clip(0, 255).byte()

    def _interpolate_concat(self, input_tensor: torch.Tensor) -> torch.Tensor:
        if self.x0_prev is None or self.x0_skip is None:
            raise RuntimeError("CHUNK_CONCAT called before x0_prev and x0_skip were cached")

        nT = self.train_num_frames // self.tile_min_t
        lq, msk, h, w = self._prepare_lq_and_msk(input_tensor, self.train_num_frames)

        y = self.model.lq_encoder.encode(lq, for_train=True)
        m = self.model.msk_encoder.encode(msk, for_train=True)

        xt_cat = torch.randn_like(y[:, self.nT_cond : nT - self.nT_cond])
        xt_prev = self.x0_prev * (1.0 - self.t_cond) + torch.randn_like(self.x0_prev) * self.t_cond
        xt_skip = self.x0_skip * (1.0 - self.t_cond) + torch.randn_like(self.x0_skip) * self.t_cond

        t_prev = torch.tensor([self.t_cond]).expand(self.x0_prev.shape[:-4])
        t_skip = torch.tensor([self.t_cond]).expand(self.x0_skip.shape[:-4])

        for i in range(self.num_steps):
            xt = torch.cat([xt_prev, xt_cat, xt_skip], dim=1)
            t_cat = torch.tensor([self.ts[i]]).expand(xt_cat.shape[:-4]).contiguous()
            t = torch.cat([t_prev, t_cat, t_skip], dim=1).to(device=self.device, dtype=self.dtype)
            vt = self.model.predict_v(xt, t, y, m)[:, self.nT_cond : nT - self.nT_cond]
            xt_cat = xt_cat + vt * (self.ts[i + 1] - self.ts[i])

        # Advance previous boundary latent to forward skip boundary
        self.x0_prev = self.x0_prev_
        self.x0_skip = None
        self.x0_prev_ = None

        xt = rearrange(xt_cat, "1 nt nh nw c t h w -> 1 nt c t (nh h) (nw w)")
        lq_sub = lq[:, :, self.nT_cond * self.tile_min_t : (nT - self.nT_cond) * self.tile_min_t]
        msk_sub = msk[:, :, self.nT_cond * self.tile_min_t : (nT - self.nT_cond) * self.tile_min_t]

        pred = vae_decode(self.model.vae, xt, lq=lq_sub, msk=msk_sub)[..., :h, :w]
        return rearrange(pred, "1 c t h w -> t c h w").add(1.0).mul(127.5).clip(0, 255).byte()

    def _interpolate_last(self, input_tensor: torch.Tensor) -> torch.Tensor:
        nT = self.train_num_frames // self.tile_min_t
        lq, msk, h, w = self._prepare_lq_and_msk(input_tensor, self.train_num_frames)

        y = self.model.lq_encoder.encode(lq, for_train=True)
        m = self.model.msk_encoder.encode(msk, for_train=True)

        if self.x0_prev is not None:
            xt_tail = torch.randn_like(y[:, self.nT_cond :])
            xt_prev = (
                self.x0_prev * (1.0 - self.t_cond) + torch.randn_like(self.x0_prev) * self.t_cond
            )
            t_prev = torch.tensor([self.t_cond]).expand(self.x0_prev.shape[:-4])

            for i in range(self.num_steps):
                xt = torch.cat([xt_prev, xt_tail], dim=1)
                t_tail = torch.tensor([self.ts[i]]).expand(xt_tail.shape[:-4]).contiguous()
                t = torch.cat([t_prev, t_tail], dim=1).to(device=self.device, dtype=self.dtype)
                vt = self.model.predict_v(xt, t, y, m)[:, self.nT_cond :]
                xt_tail = xt_tail + vt * (self.ts[i + 1] - self.ts[i])
            xt = xt_tail
            lq_sub = lq[:, :, self.nT_cond * self.tile_min_t : nT * self.tile_min_t]
            msk_sub = msk[:, :, self.nT_cond * self.tile_min_t : nT * self.tile_min_t]
        else:
            xt = torch.randn_like(y)
            xt = self._denoise_loop(xt, y, m)
            lq_sub = lq
            msk_sub = msk

        self.cleanup_latents()

        xt = rearrange(xt, "1 nt nh nw c t h w -> 1 nt c t (nh h) (nw w)")

        pred = vae_decode(self.model.vae, xt, lq=lq_sub, msk=msk_sub)[..., :h, :w]
        return rearrange(pred, "1 c t h w -> t c h w").add(1.0).mul(127.5).clip(0, 255).byte()

    def _interpolate_standalone(self, input_tensor: torch.Tensor) -> torch.Tensor:
        """Self-contained chunk without caching boundary latents."""
        lq, msk, h, w = self._prepare_lq_and_msk(input_tensor, self.train_num_frames)

        y = self.model.lq_encoder.encode(lq, for_train=True)
        m = self.model.msk_encoder.encode(msk, for_train=True)
        xt = torch.randn_like(y)

        xt = self._denoise_loop(xt, y, m)

        xt = rearrange(xt, "1 nt nh nw c t h w -> 1 nt c t (nh h) (nw w)")
        pred = vae_decode(self.model.vae, xt, lq=lq, msk=msk)[..., :h, :w]
        return rearrange(pred, "1 c t h w -> t c h w").add(1.0).mul(127.5).clip(0, 255).byte()

    def cleanup_latents(self) -> None:
        self.x0_prev = None
        self.x0_skip = None
        self.x0_prev_ = None

    def cleanup(self) -> None:
        self.cleanup_latents()
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
