import argparse
from concurrent import futures
import logging
import os
import signal
import threading
from typing import Optional
import grpc

from ldfvfi.proto.service_pb2_grpc import add_LdfVfiServiceServicer_to_server
from ldfvfi.server.model_holder import ModelHolder, create_mock_model_holder
from ldfvfi.server.model_loader import load_ldfvfi_model
from ldfvfi.server.service import LdfVfiService

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger("ldfvfi.server")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="LDF-VFI Video Frame Interpolation Daemon")
    parser.add_argument("--port", type=int, default=50051, help="TCP port to bind gRPC server")
    parser.add_argument("--uds", type=str, default=None, help="Unix domain socket path to bind gRPC server")
    parser.add_argument("--model-dir", type=str, default="models", help="Directory containing model checkpoints")
    parser.add_argument("--config-path", type=str, default=None, help="Path to DiT model configuration JSON")
    parser.add_argument("--device", type=str, default="cuda", help="Target PyTorch execution device (e.g. cuda, cpu)")
    parser.add_argument("--mock", action="store_true", help="Run with mock model on CPU (for testing without GPU)")
    parser.add_argument("--workers", type=int, default=2, help="Number of gRPC worker threads")
    return parser


def serve(args: Optional[argparse.Namespace] = None) -> None:
    if args is None:
        args = build_parser().parse_args()

    # 1. Initialize model holder
    if args.mock:
        logger.info("Initializing mock model holder on CPU")
        model_holder = create_mock_model_holder()
    else:
        logger.info(f"Loading LDF-VFI model from '{args.model_dir}' onto device '{args.device}'")
        model = load_ldfvfi_model(
            model_dir=args.model_dir,
            config_path=args.config_path,
            device=args.device,
        )
        model_holder = ModelHolder(model=model, device=args.device)

    # 2. Instantiate servicer and gRPC server
    servicer = LdfVfiService(model_holder=model_holder)
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=args.workers))
    add_LdfVfiServiceServicer_to_server(servicer, server)

    # 3. Bind listeners
    bound_endpoints = []
    if args.uds:
        uds_path = args.uds.removeprefix("unix://").removeprefix("unix:")
        if os.path.exists(uds_path):
            try:
                os.unlink(uds_path)
            except OSError as e:
                logger.warning(f"Failed to unlink existing socket file '{uds_path}': {e}")
        server.add_insecure_port(f"unix://{uds_path}")
        bound_endpoints.append(f"unix://{uds_path}")

    if args.port or not args.uds:
        server.add_insecure_port(f"0.0.0.0:{args.port}")
        bound_endpoints.append(f"0.0.0.0:{args.port}")

    # 4. Signal handling
    stop_event = threading.Event()

    def handle_signal(signum, frame):
        sig_name = signal.Signals(signum).name
        logger.info(f"Received signal {sig_name}, initiating graceful shutdown...")
        stop_event.set()

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    # 5. Start server
    server.start()
    logger.info(f"LDF-VFI daemon listening on {', '.join(bound_endpoints)}")

    # 6. Wait for shutdown signal
    stop_event.wait()

    # 7. Graceful teardown
    logger.info("Shutting down daemon...")
    servicer.reset()
    server.stop(grace=3.0).wait()

    if args.uds:
        uds_path = args.uds.removeprefix("unix://").removeprefix("unix:")
        if os.path.exists(uds_path):
            try:
                os.unlink(uds_path)
            except OSError:
                pass

    logger.info("LDF-VFI daemon stopped cleanly.")


def main() -> None:
    serve()


if __name__ == "__main__":
    main()
