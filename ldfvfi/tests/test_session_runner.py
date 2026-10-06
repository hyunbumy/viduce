import numpy as np
import pytest
import torch

from ldfvfi.proto import ChunkType, VfiParameters
from ldfvfi.server.session_runner import SkipConcatSession, compute_schedule


from ldfvfi.testing.mock_model import MockModel


@pytest.fixture
def session_params() -> VfiParameters:
    return VfiParameters(
        sampling_steps=2,
        t_shift=1.0,
        t_cond=0.1,
        temporal_scale_factor=4,
        train_num_frames=60,
        tile_min_t=20,
    )


def test_compute_schedule():
    ts = compute_schedule(num_steps=10, t_shift=1.0)
    assert len(ts) == 11
    assert np.isclose(ts[0], 1.0)
    assert np.isclose(ts[-1], 0.0)
    # Monotonically decreasing
    assert np.all(np.diff(ts) < 0)


def test_session_init(session_params):
    session = SkipConcatSession(
        session_id="test-session",
        params=session_params,
        model=MockModel(),
        device="cpu",
        dtype=torch.float32,
    )
    assert session.session_id == "test-session"
    assert session.num_steps == 2
    assert session.t_shift == 1.0
    assert session.t_cond == pytest.approx(0.1)
    assert session.x0_prev is None
    assert session.x0_skip is None
    assert session.x0_prev_ is None


def test_session_state_machine_flow(session_params):
    """Verifies state machine transitions and latent preservation across FIRST -> SKIP -> CONCAT -> LAST."""
    model = MockModel()
    session = SkipConcatSession(
        session_id="session-flow",
        params=session_params,
        model=model,
        device="cpu",
        dtype=torch.float32,
    )

    # 15 input keyframes, 3 channels, 32x32 resolution
    input_frames = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)

    # Step 1: FIRST chunk
    out_first = session.interpolate_chunk(ChunkType.CHUNK_FIRST, input_frames)
    assert out_first.dim() == 4
    assert out_first.shape[1:] == (3, 32, 32)
    assert out_first.dtype == torch.uint8
    assert session.x0_prev is not None
    assert session.x0_skip is None
    assert session.x0_prev_ is None
    saved_prev = session.x0_prev

    # Step 2: SKIP chunk
    out_skip = session.interpolate_chunk(ChunkType.CHUNK_SKIP, input_frames)
    assert out_skip.dim() == 4
    assert out_skip.shape[1:] == (3, 32, 32)
    assert out_skip.dtype == torch.uint8
    # x0_prev must be untouched from FIRST chunk
    assert torch.equal(session.x0_prev, saved_prev)
    assert session.x0_skip is not None
    assert session.x0_prev_ is not None
    saved_skip = session.x0_skip
    saved_prev_next = session.x0_prev_

    # Step 3: CONCAT chunk (infilling)
    out_concat = session.interpolate_chunk(ChunkType.CHUNK_CONCAT, input_frames)
    assert out_concat.dim() == 4
    assert out_concat.shape[1:] == (3, 32, 32)
    assert out_concat.dtype == torch.uint8
    # After CONCAT: x0_prev should advance to x0_prev_, and skip latents reset
    assert torch.equal(session.x0_prev, saved_prev_next)
    assert session.x0_skip is None
    assert session.x0_prev_ is None

    # Step 4: LAST chunk
    out_last = session.interpolate_chunk(ChunkType.CHUNK_LAST, input_frames, is_last=True)
    assert out_last.dim() == 4
    assert out_last.shape[1:] == (3, 32, 32)
    assert out_last.dtype == torch.uint8
    # Latents should be cleared after LAST chunk
    assert session.x0_prev is None
    assert session.x0_skip is None
    assert session.x0_prev_ is None


def test_session_standalone(session_params):
    model = MockModel()
    session = SkipConcatSession(
        session_id="session-standalone",
        params=session_params,
        model=model,
        device="cpu",
        dtype=torch.float32,
    )
    input_frames = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)

    out = session.interpolate_chunk(ChunkType.CHUNK_STANDALONE, input_frames)
    assert out.dim() == 4
    assert out.shape[1:] == (3, 32, 32)
    assert out.dtype == torch.uint8
    assert session.x0_prev is None
    assert session.x0_skip is None


def test_session_last_without_prev(session_params):
    """Tests CHUNK_LAST executed directly when x0_prev is None."""
    model = MockModel()
    session = SkipConcatSession(
        session_id="session-last-standalone",
        params=session_params,
        model=model,
        device="cpu",
        dtype=torch.float32,
    )
    input_frames = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)

    out = session.interpolate_chunk(ChunkType.CHUNK_LAST, input_frames, is_last=True)
    assert out.dim() == 4
    assert out.shape[1:] == (3, 32, 32)
    assert session.x0_prev is None


def test_session_concat_without_prerequisites_raises(session_params):
    model = MockModel()
    session = SkipConcatSession(
        session_id="session-err",
        params=session_params,
        model=model,
        device="cpu",
        dtype=torch.float32,
    )
    input_frames = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)

    with pytest.raises(RuntimeError, match="CHUNK_CONCAT called before"):
        session.interpolate_chunk(ChunkType.CHUNK_CONCAT, input_frames)


def test_session_unknown_chunk_type_raises(session_params):
    model = MockModel()
    session = SkipConcatSession(
        session_id="session-err2",
        params=session_params,
        model=model,
        device="cpu",
        dtype=torch.float32,
    )
    input_frames = torch.randint(0, 256, (15, 3, 32, 32), dtype=torch.uint8)

    with pytest.raises(ValueError, match="Unknown ChunkType"):
        session.interpolate_chunk(999, input_frames)  # type: ignore


def test_session_cleanup(session_params):
    model = MockModel()
    session = SkipConcatSession(
        session_id="session-cleanup",
        params=session_params,
        model=model,
        device="cpu",
        dtype=torch.float32,
    )
    session.x0_prev = torch.zeros(1)
    session.x0_skip = torch.zeros(1)
    session.x0_prev_ = torch.zeros(1)

    session.cleanup()
    assert session.x0_prev is None
    assert session.x0_skip is None
    assert session.x0_prev_ is None
