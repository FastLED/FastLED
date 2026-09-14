"""Resolve the reduced ``smoke`` example set that pull requests compile.

Pull requests compile a small, evidence-picked set of examples instead of all
~100 (FastLED/FastLED#4415, #4416). Master keeps compiling ``all``, so an
example-specific break still shows up on the post-merge sweep.

The set is:

1. every sketch in ``tests/platforms/_standard/SMOKE_SKETCHES.txt``, then
2. every example the pull request itself changes, found by diffing the PR
   head against its base branch.

Library code is compiled into ``libFastLED.a`` by every example, so any of
them catches a library error. The manifest adds the sketches that, in 90 days
of CI history, were the only ones to catch link errors, template
instantiations, sketch-level config macros and size overflows. Changed
examples are added so a PR that edits an example always compiles it.

The base branch comes from ``FASTLED_SMOKE_BASE_REF`` or, in GitHub Actions
pull_request runs, ``GITHUB_BASE_REF``. Outside a PR (no base ref) only the
manifest is used.
"""

from __future__ import annotations

import os
from pathlib import Path

from running_process import RunningProcess

from ci.standardized_smoke_sketches import load_smoke_sketches
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


SMOKE_KEYWORD = "smoke"


def _project_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent


def base_ref_from_env() -> str | None:
    """The branch to diff against, or None when this is not a PR build."""
    for name in ("FASTLED_SMOKE_BASE_REF", "GITHUB_BASE_REF"):
        value = os.environ.get(name, "").strip()
        if value:
            return value
    return None


def example_for_path(changed: str, examples_dir: Path) -> str | None:
    """Map a changed repo-relative path to the example that owns it.

    ``examples/Fx/FxSdCard/FxSdCard.ino`` and
    ``examples/AutoResearch/AutoResearchRemote.cpp`` both resolve to their
    sketch directory. Paths outside ``examples/`` or not inside a sketch
    directory (e.g. ``examples/README.md``) resolve to None.
    """
    parts = Path(changed.replace("\\", "/")).parts
    if len(parts) < 2 or parts[0] != "examples":
        return None
    root = examples_dir.parent
    candidate = root / Path(*parts[:-1])
    while candidate != examples_dir and examples_dir in candidate.parents:
        if any(candidate.glob("*.ino")):
            return candidate.relative_to(examples_dir).as_posix()
        candidate = candidate.parent
    return None


def _git(args: list[str], cwd: Path) -> tuple[int, str]:
    result = RunningProcess.run(
        ["git", *args],
        cwd=cwd,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
        check=False,
    )
    return result.returncode, result.stdout or ""


def changed_examples(base_ref: str, project_root: Path) -> list[str]:
    """Examples changed between ``origin/<base_ref>`` and HEAD.

    CI checkouts are shallow, so the base tip is fetched on demand. For a
    pull_request run HEAD is the PR merge commit, whose tree is the base plus
    the PR's changes, so a tree diff against the base tip is the PR's diff.
    Any git failure yields an empty list: the manifest still compiles, and
    master compiles everything.
    """
    try:
        code, _ = _git(
            ["fetch", "--no-tags", "--depth=1", "origin", base_ref], project_root
        )
        base = "FETCH_HEAD" if code == 0 else f"origin/{base_ref}"
        code, out = _git(
            ["diff", "--name-only", base, "HEAD", "--", "examples"], project_root
        )
        if code != 0:
            print(f"smoke: could not diff against {base_ref}; using the manifest only")
            return []
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"smoke: git unavailable ({e}); using the manifest only")
        return []

    examples_dir = project_root / "examples"
    found: list[str] = []
    for line in out.splitlines():
        example = example_for_path(line.strip(), examples_dir)
        if example is not None and example not in found:
            found.append(example)
    return found


def resolve_smoke_examples(
    project_root: Path | None = None, base_ref: str | None = None
) -> list[str]:
    """Manifest sketches followed by examples the PR changed, deduplicated."""
    root = project_root if project_root is not None else _project_root()
    examples = load_smoke_sketches(root)
    ref = base_ref if base_ref is not None else base_ref_from_env()
    if ref:
        added: list[str] = []
        for example in changed_examples(ref, root):
            if example not in examples and example not in added:
                added.append(example)
        if added:
            print(f"smoke: adding examples changed by this PR: {', '.join(added)}")
        examples.extend(added)
    return examples
