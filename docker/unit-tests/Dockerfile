# Use Debian Bookworm slim as base (~28MB vs ~75MB for Ubuntu 22.04)
# debian:bookworm-slim is glibc-based (unlike Alpine's musl) for full compatibility
FROM debian:bookworm-slim

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# ============================================================================
# OPTIMIZED DEPENDENCY LIST - Iteration 4
# ============================================================================
# The clang-tool-chain Python package provides the LLVM/Clang compiler,
# but we still need some system packages for linking and building.
#
# REMOVED (provided by uv/Python packages):
#   - python3, python3-pip, python3-venv (uv provides Python)
#   - cmake (meson/ninja handles builds)
#   - ninja-build (Python ninja package provides this)
#   - clang (clang-tool-chain provides this)
#   - pkg-config (not used by our meson builds)
#   - git (not required for uv sync or clang-tool-chain)
#
# REQUIRED (confirmed through testing):
#   - curl: for fetching uv installer
#   - ca-certificates: HTTPS support for curl
#   - libc6-dev: C runtime libraries (crt1.o, crti.o, libc, etc.)
#   - libstdc++-12-dev: C++ standard library
#   - libgcc-12-dev: GCC runtime library (libgcc_s)
#   - libxml2: Required by clang-tool-chain's bundled ld.lld
#
# NOT REQUIRED (clang-tool-chain provides these):
#   - lld: clang-tool-chain bundles ld.lld
#
# NOTE: First run will be slow due to:
#   - Python download by uv (~35 MB)
#   - Package installation (~150 packages)
#   - Clang toolchain download by clang-tool-chain (~190 MB)
#
# Install all required packages in a single layer to minimize image size
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    libc6-dev \
    libstdc++-12-dev \
    libgcc-12-dev \
    libxml2 \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# Install uv (Python package manager) - this also provides Python!
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
