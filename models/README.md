# Model Conversion Tools

This directory contains tools for converting deep learning models from PyTorch to LiteRT (TFLite) format, enabling efficient inference within the C++ engine.

## Overview

The primary focus is on the **Real-ESRGAN** upscaling model. We use `litert-torch` to convert trained PyTorch models into TFLite format, which can be executed with hardware acceleration by the engine.

## Environment Setup

This project uses [uv](https://github.com/astral-sh/uv) for Python package management. To set up the environment and install dependencies:

```bash
# From the project root
cd models
uv sync
```

## Converting Models

The `convert_model.py` script is used to convert a PyTorch Real-ESRGAN model.

### Usage

1. Obtain a PyTorch model file (e.g., `RealESRGAN_x4plus.pth`).
2. Run the conversion script:

```bash
uv run convert_model.py <path_to_pytorch_model> <output_path>
```

### Script Details

- **Architecture**: The script currently targets the `RRDBNet` architecture.
- **Weights**: It expects the PyTorch state dict to contain weights under the `params_ema` key.
- **Input Shape**: The model is traced with a fixed input shape of `(1, 3, 240, 240)`. If your input video has a different resolution, you must update the `sample_input` in `convert_model.py` and re-convert the model.

## Output

The script exports the converted model to the specified path.

This file is a direct dependency for the C++ engine and is expected at this location during engine initialization or inference tests.

## Dependencies

- `basicsr-fixed`: Provides the Real-ESRGAN architecture.
- `litert-torch`: Google's tool for PyTorch to TFLite conversion.
- `torch`: Core PyTorch libraries.
