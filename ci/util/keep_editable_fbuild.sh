# shellcheck shell=bash
# Sourced by the root wrappers before they call `uv run`.
#
# `uv run` syncs the venv to the lockfile, which silently swaps an editable
# fbuild checkout (`uv pip install -e ../fbuild`, used to test an unreleased
# fbuild) back to the PyPI wheel. When fbuild is installed editable, keep it.
_fl_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Resolve only an existing selected environment. In a conditional assignment,
# a missing directory is harmless under callers' set -e; the wrapper decides
# whether to provision its default or reject a missing explicit target.
_fl_selected_env="${UV_PROJECT_ENVIRONMENT:-.venv}"
if _fl_env_root="$(cd "$_fl_root" && cd -- "$_fl_selected_env" 2>/dev/null && pwd)"; then
    for _fl_direct_url in "$_fl_env_root"/lib/python3*/site-packages/fbuild-*.dist-info/direct_url.json \
        "$_fl_env_root"/Lib/site-packages/fbuild-*.dist-info/direct_url.json; do
        if [ -f "$_fl_direct_url" ] && grep -q '"editable": *true' "$_fl_direct_url"; then
            export UV_NO_SYNC=1
        fi
    done
fi
unset _fl_direct_url _fl_root _fl_selected_env _fl_env_root
