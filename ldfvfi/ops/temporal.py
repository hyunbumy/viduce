import torch


def upsample_temporal(x: torch.Tensor, msk: torch.Tensor, mode: str = "nearest") -> torch.Tensor:
    """Temporally upsample video frames based on a keyframe mask.

    Args:
        x: Input tensor of shape (T_in, C, H, W).
        msk: 1D boolean tensor of length T_out indicating keyframe positions.
        mode: Interpolation mode ('nearest' or 'pad').

    Returns:
        Upsampled tensor of shape (T_out, C, H, W).
    """
    C, H, W = x.shape[-3:]
    T = msk.shape[0]

    if mode == "pad":
        tmp = torch.zeros((T, C, H, W), device=x.device, dtype=x.dtype)
        tmp[msk] = x
        return tmp
    elif mode == "nearest":
        kept_indices = torch.where(msk)[0]
        positions = torch.arange(T, device=x.device)
        distances = (positions.view(-1, 1) - kept_indices.view(1, -1)).abs()
        nearest_kept = distances.argmin(dim=1)
        return x[nearest_kept]
    else:
        raise NotImplementedError(f"Unsupported temporal upsampling mode: {mode}")
