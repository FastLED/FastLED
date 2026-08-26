#!/usr/bin/env bash
set -euo pipefail

project_root="$(git rev-parse --show-toplevel)"
cd "$project_root"

uv run soldr --no-cache cargo test \
  --manifest-path ci/lint_cpp_rs/Cargo.toml \
  live_history \
  -- --ignored --nocapture
