import torch
from ldfvfi.server.model_holder import ModelHolder, create_mock_model_holder


def test_model_holder_properties():
    holder = create_mock_model_holder()
    assert holder.device == "cpu"
    assert holder.dtype == torch.float32
    assert holder.model is not None
    # Purge cache should not throw on cpu
    holder.purge_cache()
