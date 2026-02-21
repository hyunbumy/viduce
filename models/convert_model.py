import sys

import litert_torch
import torch
from basicsr.archs.rrdbnet_arch import RRDBNet


def main():
    # Convert RealESRGAN model to LiteRT
    model = RRDBNet(
        num_in_ch=3, num_out_ch=3, num_feat=64, num_block=23, num_grow_ch=32, scale=4
    )
    loadnet = torch.load(sys.argv[1], map_location="cpu")
    model.load_state_dict(loadnet["params_ema"], strict=True)

    # Sample input of (batch, channel, height, width) since that's what the
    # model expects.
    # TODO: Figure out how to not hardcode the input size
    sample_input = (torch.randn(1, 3, 240, 240),)
    edge_model = litert_torch.convert(model.eval(), sample_input)
    edge_model.export(sys.argv[2])


if __name__ == "__main__":
    main()
