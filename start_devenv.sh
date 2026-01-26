#!/bin/bash

# Build the Docker image for the development environment
podman build -t devenv . --target=base

# Run the Docker container with the current directory mounted
podman run --name devenv -v "$(pwd)":/mnt/host/viduce -itd --rm devenv bash

# Attach to the running container
podman exec -it devenv bash
