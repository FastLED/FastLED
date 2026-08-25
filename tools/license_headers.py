#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["running-process>=4.0.0", "zccache>=1.13.0"]
# ///
"""Inventory, check, and update FastLED managed source-license notices."""

import argparse
import fnmatch
import os
import platform
import re
import shutil
import stat
import sys
import tempfile
import tomllib
from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path
from typing import Any

from running_process import RunningProcess

RIPGREP_VERSION = "14.1.1"
RIPGREP_ASSETS: dict[tuple[str, str], tuple[str, str]] = {
    ("Windows", "x86_64"): (
        "ripgrep-14.1.1-x86_64-pc-windows-msvc.zip",
        "d0f534024c42afd6cb4d38907c25cd2b249b79bbe6cc1dbee8e3e37c2b6e25a1",
    ),
    ("Linux", "x86_64"): (
        "ripgrep-14.1.1-x86_64-unknown-linux-musl.tar.gz",
        "4cf9f2741e6c465ffdb7c26f38056a59e2a2544b51f7cc128ef28337eeae4d8e",
    ),
    ("Linux", "aarch64"): (
        "ripgrep-14.1.1-aarch64-unknown-linux-gnu.tar.gz",
        "c827481c4ff4ea10c9dc7a4022c8de5db34a5737cb74484d62eb94a95841ab2f",
    ),
    ("Darwin", "x86_64"): (
        "ripgrep-14.1.1-x86_64-apple-darwin.tar.gz",
        "fc87e78f7cb3fea12d69072e7ef3b21509754717b746368fd40d88963630e2b3",
    ),
    ("Darwin", "arm64"): (
        "ripgrep-14.1.1-aarch64-apple-darwin.tar.gz",
        "24ad76777745fbff131c8fbc466742b011f925bfa4fffa2ded6def23b5b937be",
    ),
}
MANAGED_MARKERS = (
    "SPDX-License-Identifier:",
    "AI LICENSE:",
    "AI agents must read that file before substantial FastLED changes.",
    "Substantial AI changes must be reported upstream with a reproducible patch.",
)
UTF8_BOM = b"\xef\xbb\xbf"


class State(StrEnum):
    CURRENT = "current"
    MISSING = "missing"
    OUTDATED = "outdated"
    EXCLUDED = "excluded"
    CONFLICT = "conflict"
    MALFORMED = "malformed"
    UNREADABLE = "unreadable"


@dataclass(frozen=True)
class Exclusion:
    pattern: str
    reason: str
    provenance: str


@dataclass(frozen=True)
class Policy:
    path: Path
    root: Path
    license_id: str
    header_version: int
    ai_document: str
    roots: tuple[str, ...]
    extensions: tuple[str, ...]
    comments: dict[str, str]
    old_license_ids: frozenset[str]
    exclusions: tuple[Exclusion, ...]


@dataclass(frozen=True)
class Finding:
    path: Path
    relative: str
    state: State
    detail: str = ""


@dataclass(frozen=True)
class FileData:
    bom: bytes
    text: str
    newline: str
    final_newline: bool
    mode: int


def _run(args: list[str], *, cwd: Path, timeout: float = 120.0) -> Any:
    return RunningProcess.run(
        args,
        cwd=str(cwd),
        check=False,
        timeout=timeout,
        capture_output=True,
        text=True,
    )


def load_policy(path: Path, profile: str) -> Policy:
    path = path.resolve()
    with path.open("rb") as handle:
        raw = tomllib.load(handle)
    if raw.get("schema_version") != 1:
        raise ValueError("unsupported header-policy schema_version")
    profiles = raw.get("profiles", {})
    if profile not in profiles:
        raise ValueError(f"unknown policy profile: {profile}")
    selected = profiles[profile]
    license_config = raw["license"]
    exclusions = tuple(
        Exclusion(item["pattern"], item["reason"], item["provenance"])
        for item in raw.get("exclusions", [])
    )
    for exclusion in exclusions:
        if not exclusion.reason.strip() or not exclusion.provenance.strip():
            raise ValueError(f"exclusion lacks reason/provenance: {exclusion.pattern}")
    comments = {str(key): str(value) for key, value in raw["comments"].items()}
    extensions = tuple(str(value) for value in selected["extensions"])
    roots = tuple(str(value) for value in selected["roots"])
    for root in roots:
        candidate = Path(root)
        if (
            not root
            or root == "."
            or bool(re.match(r"^[A-Za-z]:[\\/]", root))
            or bool(candidate.anchor)
            or candidate.is_absolute()
            or ".." in candidate.parts
        ):
            raise ValueError(f"policy root must be a normalized repository-relative path: {root}")
    missing_comments = sorted(set(extensions) - set(comments))
    if missing_comments:
        raise ValueError(f"extensions lack comment syntax: {', '.join(missing_comments)}")
    return Policy(
        path=path,
        root=path.parent,
        license_id=str(license_config["id"]),
        header_version=int(license_config["header_version"]),
        ai_document=str(license_config["ai_document"]),
        roots=roots,
        extensions=extensions,
        comments=comments,
        old_license_ids=frozenset(str(value) for value in raw.get("old_license_ids", [])),
        exclusions=exclusions,
    )


def _normalize_machine(machine: str) -> str:
    lowered = machine.lower()
    if lowered in {"amd64", "x64", "x86_64"}:
        return "x86_64"
    if lowered in {"arm64", "aarch64"}:
        return "arm64" if platform.system() == "Darwin" else "aarch64"
    return lowered


def _rg_version(path: Path, cwd: Path) -> tuple[int, ...] | None:
    result = _run([str(path), "--version"], cwd=cwd, timeout=15)
    if result.returncode != 0:
        return None
    match = re.search(r"ripgrep (\d+(?:\.\d+)+)", result.stdout)
    return tuple(int(part) for part in match.group(1).split(".")) if match else None


def resolve_ripgrep(root: Path) -> Path:
    override = os.environ.get("FASTLED_LICENSE_RG")
    system_rg = Path(override) if override else None
    if system_rg is None:
        found = shutil.which("rg")
        system_rg = Path(found) if found else None
    minimum = tuple(int(part) for part in RIPGREP_VERSION.split("."))
    if system_rg:
        system_version = _rg_version(system_rg, root)
        if system_version is not None and system_version >= minimum:
            return system_rg

    key = (platform.system(), _normalize_machine(platform.machine()))
    if key not in RIPGREP_ASSETS:
        raise RuntimeError(f"unsupported ripgrep bootstrap platform: {key[0]}/{key[1]}")
    asset, digest = RIPGREP_ASSETS[key]
    zccache = shutil.which("zccache")
    if not zccache:
        raise RuntimeError("zccache is required to download the pinned ripgrep artifact")
    cache_base = Path(
        os.environ.get("XDG_CACHE_HOME")
        or os.environ.get("LOCALAPPDATA")
        or (Path.home() / ".cache")
    )
    target = cache_base / "fastled-license" / "ripgrep" / RIPGREP_VERSION / f"{key[0]}-{key[1]}"
    executable = target / ("rg.exe" if key[0] == "Windows" else "rg")
    if executable.exists() and _rg_version(executable, root) == minimum:
        return executable
    archive = target.parent / asset
    target.parent.mkdir(parents=True, exist_ok=True)
    url = f"https://github.com/BurntSushi/ripgrep/releases/download/{RIPGREP_VERSION}/{asset}"
    result = _run(
        [
            zccache,
            "download",
            str(archive),
            "--url",
            url,
            "--sha256",
            digest,
            "--unarchive",
            str(target),
        ],
        cwd=root,
        timeout=300,
    )
    if result.returncode != 0:
        diagnostics = (result.stderr or result.stdout or "no diagnostics").strip()
        raise RuntimeError(f"ripgrep download failed: {diagnostics}")
    candidates = sorted(target.rglob("rg.exe" if key[0] == "Windows" else "rg"))
    if not candidates or _rg_version(candidates[0], root) != minimum:
        raise RuntimeError("downloaded ripgrep archive has an unexpected layout or version")
    executable.parent.mkdir(parents=True, exist_ok=True)
    if candidates[0] != executable:
        shutil.copy2(candidates[0], executable)
    if key[0] != "Windows":
        executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
    return executable


def discover_files(policy: Policy, rg: Path) -> list[Path]:
    existing_roots = [root for root in policy.roots if (policy.root / root).exists()]
    if not existing_roots:
        return []
    result = _run(
        [
            str(rg),
            "--files",
            "--hidden",
            "--no-ignore",
            "--no-messages",
            *existing_roots,
        ],
        cwd=policy.root,
    )
    if result.returncode not in (0, 1):
        raise RuntimeError(f"ripgrep inventory failed: {result.stderr.strip()}")
    extensions = set(policy.extensions)
    discovered: list[Path] = []
    for line in result.stdout.splitlines():
        relative = line.replace("\\", "/")
        extension = relative.rsplit(".", 1)[-1] if "." in relative else ""
        if extension in extensions:
            candidate = (policy.root / Path(relative)).resolve()
            if not candidate.is_relative_to(policy.root):
                raise RuntimeError(f"ripgrep returned a path outside the policy root: {relative}")
            discovered.append(candidate)
    return sorted(discovered, key=lambda path: path.as_posix().lower())


def expected_lines(policy: Policy, extension: str) -> list[str]:
    prefix = policy.comments[extension]
    return [
        f"{prefix} SPDX-License-Identifier: {policy.license_id}",
        f"{prefix} AI LICENSE: {policy.ai_document}",
        f"{prefix} AI agents must read that file before substantial FastLED changes.",
        f"{prefix} Substantial AI changes must be reported upstream with a reproducible patch.",
    ]


def _read_file(path: Path) -> FileData:
    raw = path.read_bytes()
    bom = UTF8_BOM if raw.startswith(UTF8_BOM) else b""
    payload = raw[len(bom) :]
    text = payload.decode("utf-8")
    newline = "\r\n" if b"\r\n" in payload else "\n"
    final_newline = payload.endswith((b"\n", b"\r"))
    return FileData(bom, text, newline, final_newline, stat.S_IMODE(path.stat().st_mode))


def _is_excluded(relative: str, policy: Policy) -> Exclusion | None:
    for exclusion in policy.exclusions:
        if fnmatch.fnmatchcase(relative, exclusion.pattern):
            return exclusion
    return None


def _leading_preamble(lines: list[str], comment_prefix: str) -> list[str]:
    """Return the complete leading comment/legal preamble."""
    preamble: list[str] = []
    in_block_comment = False
    for index, line in enumerate(lines):
        stripped = line.lstrip()
        if index == 0 and stripped.startswith("#!"):
            preamble.append(line)
            continue
        if in_block_comment:
            preamble.append(line)
            if "*/" in stripped:
                in_block_comment = False
            continue
        if not stripped or stripped.startswith(comment_prefix):
            preamble.append(line)
            continue
        if stripped.startswith("/*"):
            preamble.append(line)
            in_block_comment = "*/" not in stripped
            continue
        break
    return preamble


def classify(path: Path, policy: Policy) -> Finding:
    relative = path.relative_to(policy.root).as_posix()
    exclusion = _is_excluded(relative, policy)
    if exclusion:
        return Finding(path, relative, State.EXCLUDED, exclusion.reason)
    extension = relative.rsplit(".", 1)[-1]
    try:
        data = _read_file(path)
    except (OSError, UnicodeDecodeError) as error:
        return Finding(path, relative, State.UNREADABLE, str(error))
    lines = data.text.splitlines()
    expected = expected_lines(policy, extension)
    preamble = _leading_preamble(lines, policy.comments[extension])
    current_block = any(
        preamble[index : index + len(expected)] == expected
        for index in range(max(0, len(preamble) - len(expected) + 1))
    )
    spdx_lines = [line for line in preamble if "SPDX-License-Identifier:" in line]
    identifiers = [
        line.split("SPDX-License-Identifier:", 1)[1].strip() for line in spdx_lines
    ]
    if current_block:
        if identifiers == [policy.license_id]:
            return Finding(path, relative, State.CURRENT)
        return Finding(path, relative, State.CONFLICT, ", ".join(identifiers))
    if spdx_lines:
        if len(identifiers) != 1:
            return Finding(path, relative, State.CONFLICT, ", ".join(identifiers))
        identifier = identifiers[0]
        if identifier in policy.old_license_ids:
            return Finding(path, relative, State.OUTDATED, identifier)
        return Finding(path, relative, State.CONFLICT, identifier)
    if any(marker in line for marker in MANAGED_MARKERS[1:] for line in preamble):
        return Finding(path, relative, State.MALFORMED, "partial managed notice")
    return Finding(path, relative, State.MISSING)


def inventory(policy: Policy, rg: Path) -> list[Finding]:
    return [classify(path, policy) for path in discover_files(policy, rg)]


def _insertion_index(lines: list[str]) -> int:
    index = 0
    if lines and lines[0].startswith("#!"):
        index = 1
    if index < len(lines) and re.match(r"^#.*coding[:=]\s*[-\w.]+", lines[index]):
        index += 1
    return index


def _render(data: FileData, lines: list[str]) -> bytes:
    text = data.newline.join(lines)
    if data.final_newline:
        text += data.newline
    return data.bom + text.encode("utf-8")


def update_file(finding: Finding, policy: Policy, *, dry_run: bool = False) -> bool:
    if finding.state in {State.CURRENT, State.EXCLUDED}:
        return False
    if finding.state not in {State.MISSING, State.OUTDATED}:
        raise ValueError(f"refusing to rewrite {finding.relative}: {finding.state.value}")
    data = _read_file(finding.path)
    lines = data.text.splitlines()
    extension = finding.relative.rsplit(".", 1)[-1]
    replacement = expected_lines(policy, extension)
    if finding.state is State.MISSING:
        index = _insertion_index(lines)
        lines[index:index] = replacement + ([""] if lines[index:] else [])
    else:
        index = next(i for i, line in enumerate(lines) if "SPDX-License-Identifier:" in line)
        end = index + 1
        while end < min(len(lines), index + 4) and any(
            marker in lines[end] for marker in MANAGED_MARKERS[1:]
        ):
            end += 1
        lines[index:end] = replacement
    updated = _render(data, lines)
    if updated == finding.path.read_bytes() or dry_run:
        return updated != finding.path.read_bytes()
    with tempfile.NamedTemporaryFile(dir=finding.path.parent, delete=False) as handle:
        temporary = Path(handle.name)
        handle.write(updated)
    try:
        temporary.chmod(data.mode)
        os.replace(temporary, finding.path)
    finally:
        if temporary.exists():
            temporary.unlink()
    return True


def _cache_file(policy: Policy, profile: str) -> Path:
    return policy.root / ".cache" / "license-headers" / f"{profile}.json"


def fingerprint(policy: Policy, profile: str, command: str) -> int:
    zccache = shutil.which("zccache")
    if not zccache:
        raise RuntimeError("zccache is required for compliance fingerprints")
    cache_file = _cache_file(policy, profile)
    cache_file.parent.mkdir(parents=True, exist_ok=True)
    args = [
        zccache,
        "fp",
        "--cache-file",
        str(cache_file),
        "--cache-type",
        "hash",
        command,
    ]
    if command == "check":
        args.extend(["--root", str(policy.root.resolve())])
        for root in policy.roots:
            for extension in policy.extensions:
                args.extend(["--include", f"{root}/**/*.{extension}"])
        for name in (
            policy.path.name,
            "LICENSE",
            "LICENSE-AI-AGENT-INSTRUCTIONS.md",
            "NOTICE-TEMPLATE.txt",
        ):
            args.extend(["--include", name])
        try:
            script_relative = Path(__file__).resolve().relative_to(policy.root).as_posix()
            args.extend(["--include", script_relative])
        except ValueError:
            pass
    result = _run(args, cwd=policy.root)
    if command == "check" and result.returncode in (0, 1):
        return int(result.returncode)
    if result.returncode != 0:
        raise RuntimeError(f"zccache fingerprint {command} failed: {result.stderr.strip()}")
    return 0


def mark_success_stably(policy: Policy, profile: str, rg: Path) -> bool:
    """Commit a successful scan and reverify if the fingerprint advances.

    Fresh zccache roots can report one content transition immediately after
    their first mark. Never bless that newer state blindly: rescan it first.
    """
    fingerprint(policy, profile, "mark-success")
    if fingerprint(policy, profile, "check") == 1:
        return True
    refreshed = inventory(policy, rg)
    violations = [
        finding
        for finding in refreshed
        if finding.state not in {State.CURRENT, State.EXCLUDED}
    ]
    if violations:
        fingerprint(policy, profile, "mark-failure")
        _print_findings(refreshed)
        return False
    fingerprint(policy, profile, "mark-success")
    return True


def _print_findings(findings: list[Finding]) -> None:
    counts: dict[State, int] = {state: 0 for state in State}
    for finding in findings:
        counts[finding.state] += 1
        if finding.state not in {State.CURRENT, State.EXCLUDED}:
            suffix = f" ({finding.detail})" if finding.detail else ""
            print(f"{finding.state.value}: {finding.relative}{suffix}")
    print(" ".join(f"{state.value}={counts[state]}" for state in State))


def execute(args: argparse.Namespace) -> int:
    policy = load_policy(Path(args.policy), args.profile)
    use_cache = not args.no_cache and args.command == "check"
    if use_cache and fingerprint(policy, args.profile, "check") == 1:
        print(f"license headers compliant (cached, profile={args.profile})")
        return 0
    rg = resolve_ripgrep(policy.root)
    findings = inventory(policy, rg)
    _print_findings(findings)
    blocking = [
        finding
        for finding in findings
        if finding.state not in {State.CURRENT, State.EXCLUDED, State.MISSING, State.OUTDATED}
    ]
    if args.command == "inventory":
        return 1 if blocking else 0
    if args.command == "check":
        violations = [
            finding
            for finding in findings
            if finding.state not in {State.CURRENT, State.EXCLUDED}
        ]
        if violations:
            if use_cache:
                fingerprint(policy, args.profile, "mark-failure")
            return 1
        return 0 if not use_cache or mark_success_stably(policy, args.profile, rg) else 1
    if blocking:
        return 1
    changed = 0
    for finding in findings:
        changed += int(update_file(finding, policy, dry_run=args.dry_run))
    print(f"updated={changed} dry_run={args.dry_run}")
    if args.dry_run:
        return 1 if changed else 0
    if not args.no_cache:
        fingerprint(policy, args.profile, "invalidate")
    verified = inventory(policy, rg)
    violations = [
        finding
        for finding in verified
        if finding.state not in {State.CURRENT, State.EXCLUDED}
    ]
    if violations:
        _print_findings(verified)
        return 1
    if not args.no_cache:
        fingerprint(policy, args.profile, "check")
        if not mark_success_stably(policy, args.profile, rg):
            return 1
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inventory", "check", "update", "apply"))
    parser.add_argument("--profile", default="release")
    parser.add_argument("--policy", default="header-policy.toml")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--no-cache", action="store_true")
    return parser


def main() -> int:
    try:
        args = build_parser().parse_args()
        return execute(args)
    except KeyboardInterrupt:
        raise
    except Exception as error:
        print(f"license header tool failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
