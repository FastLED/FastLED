#!/usr/bin/env bash
set -euo pipefail

project_root="$(git rev-parse --show-toplevel)"
cd "$project_root"

uv run soldr --no-cache cargo test \
  --manifest-path ci/lint_cpp_rs/Cargo.toml \
  deep_live_history_matches_per_file_git_for_mark_and_sam \
  -- --ignored --nocapture
