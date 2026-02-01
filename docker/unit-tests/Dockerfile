# Use Debian Bookworm slim as base (~28MB vs ~75MB for Ubuntu 22.04)
# debian:bookworm-slim is glibc-based (unlike Alpine's musl) for full compatibility
FROM debian:bookworm-slim

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required dependencies in a single layer to minimize image size
# Using --no-install-recommends to skip optional packages
RUN apt-get update && apt-get install -y --no-install-recommends \
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
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

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
