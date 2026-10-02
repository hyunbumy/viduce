import json
import os
from pathlib import Path
from typing import Optional
import torch

from ldfvfi.models.msk_encoder import MaskEncoder
from ldfvfi.models.precond import (
    Precond,
    Wan2_1SpatialTiledConditionEncoder3Dv2,
)
from ldfvfi.models.transformer_wan import WanTransformer3DModel
from ldfvfi.models.vae2_1_cond_v2 import WanVAE as WanVAECondition_v2


def load_ldfvfi_model(
    model_dir: str,
    config_path: Optional[str] = None,
    device: str = "cuda",
    dtype: torch.dtype = torch.bfloat16,
) -> Precond:
    """Instantiates and loads weights for LDF-VFI preconditioned model."""
    if config_path is None:
        # Resolve ldfvfi/config/wan1_3B.json
        cur_file = Path(__file__).resolve()
        # ldfvfi/src/ldfvfi/server/model_loader.py -> parents[3] is ldfvfi/
        candidate = cur_file.parents[3] / "config" / "wan1_3B.json"
        if candidate.exists():
            config_path = str(candidate)
        else:
            config_path = "config/wan1_3B.json"

    with open(config_path, "r") as f:
        config = json.load(f)

    transformer = WanTransformer3DModel.from_config(config)

    # Check for weights file in model_dir
    dit_weights = os.path.join(model_dir, "diffusion_pytorch_model.safetensors")
    if not os.path.exists(dit_weights):
        dit_weights = os.path.join(model_dir, "diffusion_pytorch_model.bin")
    if os.path.exists(dit_weights):
        if dit_weights.endswith(".safetensors"):
            from safetensors.torch import load_file
            state_dict = load_file(dit_weights)
        else:
            state_dict = torch.load(dit_weights, map_location="cpu")
        transformer.load_state_dict(state_dict, strict=False)

    vae = WanVAECondition_v2()
    lq_encoder = Wan2_1SpatialTiledConditionEncoder3Dv2(vae)
    msk_encoder = MaskEncoder()

    model = Precond(
        transformer=transformer,
        vae=vae,
        lq_encoder=lq_encoder,
        msk_encoder=msk_encoder,
    )
    model.eval()
    model.requires_grad_(False)
    model.to(device=device, dtype=dtype)
    return model
