#!/usr/bin/env python3
"""Limit GitHub Actions pull_request triggers to the agent PR allowlist.

By default this script is a dry run. Pass --apply to rewrite workflow files.
It intentionally leaves existing push, paths, and workflow_dispatch settings
alone; the repo's workflow shape was already mostly correct.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_PR_ALLOWLIST = frozenset(
    {
        "unit_test_linux.yml",
        "example_test_linux.yml",
        "build_uno.yml",
        "build_esp32s3.yml",
        "build_teensy41.yml",
        "build_wasm.yml",
    }
)

EVENT_RE = re.compile(r"^([A-Za-z0-9_-]+):(?:\s*(?:#.*)?)?$")


@dataclass(frozen=True)
class EventBlock:
    name: str
    start: int
    end: int
    indent: int


@dataclass(frozen=True)
class OnBlock:
    start: int
    end: int
    child_indent: int
    events: dict[str, EventBlock]


@dataclass(frozen=True)
class WorkflowResult:
    path: Path
    changes: tuple[str, ...]
    original_text: str
    updated_text: str

    @property
    def changed(self) -> bool:
        return self.original_text != self.updated_text


def line_indent(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def find_on_block(lines: list[str]) -> OnBlock | None:
    on_start = None
    on_indent = 0
    for index, line in enumerate(lines):
        if line.strip() == "on:":
            on_start = index
            on_indent = line_indent(line)
            break

    if on_start is None:
        return None

    on_end = len(lines)
    for index in range(on_start + 1, len(lines)):
        stripped = lines[index].strip()
        if not stripped or stripped.startswith("#"):
            continue
        if line_indent(lines[index]) <= on_indent:
            on_end = index
            break

    child_indent = 2
    for index in range(on_start + 1, on_end):
        stripped = lines[index].strip()
        if not stripped or stripped.startswith("#"):
            continue
        indent = line_indent(lines[index])
        if indent > on_indent:
            child_indent = indent
            break

    event_starts: list[tuple[str, int]] = []
    for index in range(on_start + 1, on_end):
        if line_indent(lines[index]) != child_indent:
            continue
        match = EVENT_RE.match(lines[index].strip())
        if match:
            event_starts.append((match.group(1), index))

    events: dict[str, EventBlock] = {}
    for position, (name, start) in enumerate(event_starts):
        end = event_starts[position + 1][1] if position + 1 < len(event_starts) else on_end
        events[name] = EventBlock(name=name, start=start, end=end, indent=child_indent)

    return OnBlock(start=on_start, end=on_end, child_indent=child_indent, events=events)


def remove_pull_request_if_needed(path: Path, allow_pr: bool) -> WorkflowResult:
    original_text = path.read_text(encoding="utf-8")
    if allow_pr:
        return WorkflowResult(path, (), original_text, original_text)

    lines = original_text.splitlines(keepends=True)
    on_block = find_on_block(lines)
    if on_block is None:
        return WorkflowResult(path, (), original_text, original_text)

    pull_request = on_block.events.get("pull_request")
    if pull_request is None:
        return WorkflowResult(path, (), original_text, original_text)

    updated_lines = lines[: pull_request.start] + lines[pull_request.end :]
    return WorkflowResult(
        path=path,
        changes=("remove pull_request",),
        original_text=original_text,
        updated_text="".join(updated_lines),
    )


def workflow_files(workflows_dir: Path) -> list[Path]:
    files = [*workflows_dir.glob("*.yml"), *workflows_dir.glob("*.yaml")]
    return sorted(path for path in files if path.is_file())


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Limit GitHub Actions pull_request triggers to the agent PR allowlist."
    )
    parser.add_argument(
        "--workflows-dir",
        type=Path,
        default=Path(".github/workflows"),
        help="Directory containing workflow .yml/.yaml files.",
    )
    parser.add_argument(
        "--allow-pr",
        action="append",
        default=[],
        metavar="FILENAME",
        help="Additional workflow filename allowed to keep pull_request.",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Rewrite workflow files. Without this flag, only print the planned changes.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Return a non-zero exit code if any workflow would change.",
    )
    parser.add_argument(
        "--print-allowlist",
        action="store_true",
        help="Print the effective PR allowlist and exit.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    allowlist = DEFAULT_PR_ALLOWLIST | set(args.allow_pr)

    if args.print_allowlist:
        for filename in sorted(allowlist):
            print(filename)
        return 0

    workflows_dir = args.workflows_dir
    if not workflows_dir.is_dir():
        print(f"Workflow directory not found: {workflows_dir}", file=sys.stderr)
        return 2

    results = [
        remove_pull_request_if_needed(path, allow_pr=path.name in allowlist)
        for path in workflow_files(workflows_dir)
    ]
    changed = [result for result in results if result.changed]

    for result in changed:
        print(result.path.as_posix())
        for change in result.changes:
            print(f"  - {change}")

    if args.apply:
        for result in changed:
            result.path.write_text(result.updated_text, encoding="utf-8")
        print(f"Updated {len(changed)} workflow file(s).")
    else:
        print(f"Would update {len(changed)} workflow file(s). Pass --apply to rewrite.")

    if args.check and changed:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
