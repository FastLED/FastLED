# Use Debian Bookworm slim as base (~28MB vs ~75MB for Ubuntu 22.04)
# debian:bookworm-slim is glibc-based (unlike Alpine's musl) for full compatibility
FROM debian:bookworm-slim

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# ============================================================================
# OPTIMIZED LAYER STRUCTURE FOR MAXIMUM CACHING
# ============================================================================
# Layer 1: System packages (rarely changes)
# Layer 2: uv installation (rarely changes)
# Layer 3: pyproject.toml copy (changes when dependencies change)
# Layer 4: Python dependency installation (cached unless pyproject.toml changes)
# Layer 5: Clang toolchain pre-installation (cached unless versions change)
#
# Source code is NOT included in the image - it's volume-mounted at runtime.
#
# This structure ensures that:
# - Source code changes NEVER invalidate Docker layers (instant)
# - Dependency changes only invalidate layers 4-5 (medium rebuild)
# - System package changes invalidate layers 2-5 (full rebuild)
#
# The image acts as a pre-warmed environment with all tools and dependencies
# cached, while the actual source code comes from the host filesystem.

# ============================================================================
# LAYER 1: System packages (cached until package list changes)
# ============================================================================
# REQUIRED packages:
#   - curl: for fetching uv installer
#   - ca-certificates: HTTPS support for curl
#   - libc6-dev: C runtime libraries (crt1.o, crti.o, libc, etc.)
#   - libstdc++-12-dev: C++ standard library
#   - libgcc-12-dev: GCC runtime library (libgcc_s)
#   - libxml2: Required by clang-tool-chain's bundled ld.lld
#   - valgrind: For callgrind profiling and analysis
#   - binutils: Provides strip command for removing debug symbols
#   - rsync: For content-based file syncing (prevents timestamp rebuild issues)
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    libc6-dev \
    libstdc++-12-dev \
    libgcc-12-dev \
    libxml2 \
    valgrind \
    binutils \
    rsync \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# ============================================================================
# LAYER 2: Install uv (cached until uv version changes)
# ============================================================================
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/root/.local/bin:${PATH}"

# Set working directory
WORKDIR /fastled

# ============================================================================
# LAYER 3: Copy dependency specification (cached until pyproject.toml changes)
# ============================================================================
COPY pyproject.toml README.md ./

# ============================================================================
# LAYER 4: Install Python dependencies (cached unless pyproject.toml changes)
# ============================================================================
# Use --no-install-project to skip installing local 'ci' package
# (source code not copied yet, will be installed in Layer 7)
RUN uv sync --no-install-project

# ============================================================================
# LAYER 5: Pre-install clang-tool-chain (~190MB download, cached)
# ============================================================================
# Trigger clang installation to cache the download (~190MB)
# This layer is only invalidated if the clang-tool-chain version changes
# NOTE: LLDB pre-install skipped due to upstream 404 error for lldb-21.1.5
#       LLDB will be installed on-demand if needed (not required for builds)
RUN uv run clang-tool-chain-cpp --version

# ============================================================================
# NOTE: Source code is NOT copied into the image
# ============================================================================
# Source code (src/, ci/, tests/, examples/) is volume-mounted at runtime
# by docker_runner.py. This ensures that:
# 1. Source code changes don't invalidate Docker image layers
# 2. The Docker image stays small and builds fast
# 3. Local edits are immediately visible in the container
#
# The 'ci' package installation happens at runtime via `uv sync` in the
# docker run command (after source code is volume-mounted).

# Default command
CMD ["/bin/bash"]
