# Use Ubuntu 22.04 LTS as base
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    python3-venv \
    clang \
    lld \
    ninja-build \
    pkg-config \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install uv (Python package manager)
RUN curl -LsSf https://astral.sh/uv/install.sh | sh

# Add uv to PATH
ENV PATH="/root/.local/bin:${PATH}"

# Set working directory
WORKDIR /fastled

# NOTE: Files are NOT copied into the image.
# Source code is mounted via volume at runtime for faster builds.
# The .venv and .build directories use named volumes for persistence.

# Default command
CMD ["/bin/bash"]
