#include <signal.h>

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "engine/engine_service_impl.h"
#include "grpcpp/grpcpp.h"
#include "spdlog/spdlog.h"

ABSL_FLAG(std::string, grpc_address, "0.0.0.0:50051",
          "Address the EngineService gRPC server listens on.");

namespace {

// Shutdown is driven by sigwait() rather than a std::signal handler. A handler
// may only set a sig_atomic_t or lock-free atomic -- it cannot call
// Server::Shutdown() or log -- and runs on an arbitrary thread among gRPC's,
// so it would need a polling loop. sigwait() instead delivers the signal
// synchronously on the main thread, where shutting down and logging are safe.
// It is POSIX-only, which is fine for this Linux-only engine.

// Blocks SIGINT and SIGTERM. Must run before gRPC spawns its threads, which
// inherit the mask, so that the signals reach only WaitForShutdownSignal().
sigset_t BlockShutdownSignals() {
  sigset_t signals;
  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &signals, nullptr);
  return signals;
}

// Waits until one of the blocked shutdown signals arrives and returns it.
int WaitForShutdownSignal(const sigset_t& signals) {
  int signal_number = 0;
  sigwait(&signals, &signal_number);
  return signal_number;
}

}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const std::string address = absl::GetFlag(FLAGS_grpc_address);

  const sigset_t signals = BlockShutdownSignals();

  viduce::engine::EngineServiceImpl service;
  grpc::ServerBuilder builder;
  // Without this gRPC sets SO_REUSEPORT and silently shares a port in use.
  builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  if (server == nullptr) {
    spdlog::error("engine_server failed to listen on {}", address);
    return 1;
  }
  spdlog::info("engine_server listening on {}", address);

  const int signal_number = WaitForShutdownSignal(signals);
  spdlog::info("Received signal {}, shutting down", signal_number);
  server->Shutdown();
  server->Wait();
  spdlog::info("engine_server stopped");
  return 0;
}
