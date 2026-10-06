import torch
from ldfvfi.testing.mock_model import MockModel, create_mock_model


def test_create_mock_model():
    model = create_mock_model()
    assert isinstance(model, MockModel)
    xt = torch.zeros((1, 3, 16, 5, 4, 4))
    pred = model.predict_v(xt, torch.tensor([0.5]), None, None)
    assert pred.shape == xt.shape
