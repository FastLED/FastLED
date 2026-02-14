# Use Debian Bookworm slim as base (~28MB vs ~75MB for Ubuntu 22.04)
# debian:bookworm-slim is glibc-based (unlike Alpine's musl) for full compatibility
FROM debian:bookworm-slim

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# ============================================================================
# OPTIMIZED LAYER STRUCTURE FOR FAST REBUILDS
# ============================================================================
# Layer 1: System packages (rarely changes)
# Layer 2: uv installation (rarely changes)
# Layer 3: pyproject.toml copy (changes when dependencies change)
# Layer 4: Python dependency installation (cached unless pyproject.toml changes)
# Layer 5: Clang toolchain pre-installation (cached unless versions change)
# Layer 6: Source code copy (changes frequently, but all deps already cached)
# Layer 7: Local package installation (fast since deps cached)
#
# This structure ensures that:
# - Source code changes only invalidate layers 6-7 (fast rebuild)
# - Dependency changes invalidate layers 4-7 (medium rebuild)
# - System package changes invalidate layers 2-7 (full rebuild)

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
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    libc6-dev \
    libstdc++-12-dev \
    libgcc-12-dev \
    libxml2 \
    valgrind \
    binutils \
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
# LAYER 6: Copy source code (invalidates when source changes)
# ============================================================================
COPY ci/ ./ci/
COPY src/ ./src/

# ============================================================================
# LAYER 7: Install local 'ci' package (fast, deps already cached)
# ============================================================================
RUN uv sync

# Default command
CMD ["/bin/bash"]
