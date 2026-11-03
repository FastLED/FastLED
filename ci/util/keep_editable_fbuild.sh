# shellcheck shell=bash
# Sourced by the root wrappers before they call `uv run`.
#
# `uv run` syncs the venv to the lockfile, which silently swaps an editable
# fbuild checkout (`uv pip install -e ../fbuild`, used to test an unreleased
# fbuild) back to the PyPI wheel. When fbuild is installed editable, keep it.
_fl_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
for _fl_direct_url in "$_fl_root"/.venv/lib/python3*/site-packages/fbuild-*.dist-info/direct_url.json \
    "$_fl_root"/.venv/Lib/site-packages/fbuild-*.dist-info/direct_url.json; do
    if [ -f "$_fl_direct_url" ] && grep -q '"editable": *true' "$_fl_direct_url"; then
        export UV_NO_SYNC=1
    fi
done
unset _fl_direct_url _fl_root
