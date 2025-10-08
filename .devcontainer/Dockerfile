FROM mcr.microsoft.com/devcontainers/python:1-3.11-bullseye

# Install system dependencies for embedded development and QEMU
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    build-essential \
    clang-format \
    cmake \
    ninja-build \
    git-lfs \
    qemu-system \
    qemu-user \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*

# Install UV package manager
RUN pip install --no-cache-dir uv
