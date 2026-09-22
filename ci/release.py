"""FastLED release helper. Run through the wrapper: ``bash release <command>``.

Commands:
  status   What the newest tag, GitHub, the Arduino index and the PlatformIO
           registry each have. Read-only.
  check    The in-tree version strings are in a releasable state. Read-only.
  prepare  Bump every version string one step past the newest tag and make the
           release-notes heading final. With --pr: branch, commit, push, open
           the release PR. Merging that PR is the release.
  notes    Print one version's section of release_notes.md.
  check-tag  A pushed X.Y.Z tag names the version the tree has, with final
           notes. Runs on tag push (see .github/workflows/check_tag.yml).

How a version reaches each registry -- nothing here uploads anything:
  * Arduino indexes git tags.
  * The PlatformIO registry's legacy crawler publishes whatever version
    library.json / library.properties show on the DEFAULT BRANCH. No tag or
    GitHub release is needed to trigger it; a tag named after the version only
    decides which tree gets packaged (without one it packages the branch).

So master must never show a version that is not tagged: the crawler would ship
unreleased master under that number. ``check`` enforces that. The release PR is
the one place the version moves ahead (``check --releasing``), and its merge
commit has to be tagged straight away.
"""

import argparse
import gzip
import json
import os
import re
import sys
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from running_process import CalledProcessError, RunningProcess
from typeguard import typechecked


PROJECT_ROOT = Path(__file__).resolve().parent.parent

REGISTRY_URL = "https://api.registry.platformio.org/v3/packages/fastled/library/FastLED"
ARDUINO_INDEX_URL = "https://downloads.arduino.cc/libraries/library_index.json.gz"

_TAG_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")


@typechecked
@dataclass(frozen=True, order=True)
class Version:
    major: int
    minor: int
    patch: int

    @staticmethod
    def parse(text: str) -> "Version":
        m = _TAG_RE.match(text.strip())
        if m is None:
            raise ValueError(f"not an X.Y.Z version: {text!r}")
        return Version(int(m.group(1)), int(m.group(2)), int(m.group(3)))

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}"

    def as_int(self) -> int:
        """FASTLED_VERSION encoding: 1 digit major, 3 minor, 3 patch."""
        return self.major * 1_000_000 + self.minor * 1_000 + self.patch

    def next_steps(self) -> set["Version"]:
        """The only versions allowed to follow this one."""
        return {
            Version(self.major, self.minor, self.patch + 1),
            Version(self.major, self.minor + 1, 0),
            Version(self.major + 1, 0, 0),
        }


@typechecked
@dataclass(frozen=True)
class VersionSite:
    path: str
    value: str  # as X.Y.Z, or a description of what was found instead


def tree_version_sites(root: Path) -> list[VersionSite]:
    """Every place the tree states its version, normalized to X.Y.Z."""
    sites: list[VersionSite] = []

    props = (root / "library.properties").read_text(encoding="utf-8")
    m = re.search(r"^version=(.+)$", props, re.MULTILINE)
    sites.append(
        VersionSite("library.properties", m.group(1).strip() if m else "missing")
    )

    manifest = json.loads((root / "library.json").read_text(encoding="utf-8"))
    sites.append(VersionSite("library.json", str(manifest.get("version", "missing"))))

    header = (root / "src" / "FastLED.h").read_text(encoding="utf-8")
    m = re.search(r"^#define FASTLED_VERSION (\d+)$", header, re.MULTILINE)
    if m:
        n = int(m.group(1))
        sites.append(
            VersionSite(
                "src/FastLED.h",
                str(Version(n // 1_000_000, n // 1_000 % 1_000, n % 1_000)),
            )
        )
    else:
        sites.append(VersionSite("src/FastLED.h", "missing"))

    # Shown when a sketch defines FASTLED_SHOW_VERSION; zero-padded X.YYY.ZZZ.
    shown = re.findall(r"FastLED version (\d+)\.(\d{3})\.(\d{3})", header)
    shown_versions = {str(Version(int(a), int(b), int(c))) for a, b, c in shown}
    sites.append(
        VersionSite(
            "src/FastLED.h (FASTLED_SHOW_VERSION)",
            shown_versions.pop() if len(shown_versions) == 1 else "missing/mixed",
        )
    )

    doxyfile = (root / "docs" / "Doxyfile").read_text(encoding="utf-8")
    m = re.search(r"^PROJECT_NUMBER\s*=\s*(\S+)", doxyfile, re.MULTILINE)
    sites.append(VersionSite("docs/Doxyfile", m.group(1) if m else "missing"))
    return sites


@typechecked
@dataclass(frozen=True)
class NotesHeading:
    version: str  # X.Y.Z, or "missing"
    is_next_release: bool  # heading carries the "(Next Release)" marker


def notes_heading(root: Path) -> NotesHeading:
    """The first ``FastLED X.Y.Z`` heading in release_notes.md."""
    notes = (root / "release_notes.md").read_text(encoding="utf-8")
    # A setext heading: the version line followed by its `=` underline, so a
    # prose line that happens to start "FastLED X.Y.Z" cannot stand in for it.
    m = re.search(
        r"^FastLED (\d+\.\d+\.\d+)([^\r\n]*)\r?\n=+[ \t]*$", notes, re.MULTILINE
    )
    if m is None:
        return NotesHeading("missing", False)
    return NotesHeading(m.group(1), "(Next Release)" in m.group(2))


# Variables that point git at a repository other than the one under `cwd`.
_GIT_LOCATION_VARS = (
    "GIT_DIR",
    "GIT_WORK_TREE",
    "GIT_COMMON_DIR",
    "GIT_INDEX_FILE",
    "GIT_OBJECT_DIRECTORY",
    "GIT_ALTERNATE_OBJECT_DIRECTORIES",
)


def _git_env() -> dict[str, str]:
    """The caller's environment without repository overrides, so tag queries
    read the checkout at `root` and nothing else."""
    return {k: v for k, v in os.environ.items() if k not in _GIT_LOCATION_VARS}


def _git(args: list[str], root: Path) -> str:
    result = RunningProcess.run(
        ["git", *args],
        cwd=str(root),
        env=_git_env(),
        check=True,
        timeout=120,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    return result.stdout or ""


def release_tags(root: Path, merged_only: bool) -> list[Version]:
    args = ["tag", "--merged", "HEAD"] if merged_only else ["tag"]
    try:
        listing = _git(args, root)
    except CalledProcessError:
        return []  # not a git checkout (e.g. a ZIP download)
    tags = [t for t in listing.split() if _TAG_RE.match(t)]
    return sorted(Version.parse(t) for t in tags)


def _fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=60) as response:  # noqa: S310 - fixed https URLs
        return response.read()


def registry_versions() -> list[Version]:
    data = json.loads(_fetch(REGISTRY_URL))
    return sorted(Version.parse(v["name"]) for v in data.get("versions", []))


def arduino_versions() -> list[Version]:
    data = json.loads(gzip.decompress(_fetch(ARDUINO_INDEX_URL)))
    return sorted(
        Version.parse(lib["version"])
        for lib in data["libraries"]
        if lib["name"] == "FastLED" and _TAG_RE.match(lib["version"])
    )


class GitHubQueryError(RuntimeError):
    """`gh` failed for a reason other than the release not existing."""


def github_has_release(tag: Version, root: Path) -> bool:
    """True if GitHub has a release for `tag`, False if it has none.

    Any other `gh` failure -- not logged in, no network, rate limited -- raises
    GitHubQueryError rather than reading as "missing", which would send a
    maintainer to create a release that already exists.
    """
    result = RunningProcess.run(
        ["gh", "release", "view", str(tag), "--json", "tagName"],
        cwd=str(root),
        check=False,
        timeout=60,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode == 0:
        return True
    detail = f"{result.stderr or ''}{result.stdout or ''}".strip()
    if "release not found" in detail.lower():
        return False
    raise GitHubQueryError(
        f"gh release view {tag} failed (exit {result.returncode}): {detail}"
    )


def cmd_status(root: Path) -> int:
    tags = release_tags(root, merged_only=False)
    if not tags:
        print("no X.Y.Z tags found (shallow clone? run: git fetch --tags)")
        return 1
    newest = tags[-1]
    try:
        has_release = github_has_release(newest, root)
    except GitHubQueryError as exc:
        print(f"cannot query GitHub releases: {exc}")
        return 1
    arduino = arduino_versions()
    registry = registry_versions()
    if not arduino or not registry:
        empty = [
            name
            for name, v in (
                ("Arduino index", arduino),
                ("PlatformIO registry", registry),
            )
            if not v
        ]
        print(f"no FastLED X.Y.Z versions found in: {', '.join(empty)}")
        return 1
    rows = [
        ("newest tag", str(newest)),
        ("tree (library.properties)", tree_version_sites(root)[0].value),
        ("GitHub release for tag", "yes" if has_release else "MISSING"),
        ("Arduino index", str(arduino[-1])),
        ("PlatformIO registry", str(registry[-1])),
    ]
    for label, value in rows:
        print(f"  {label:<28} {value}")
    behind = [label for label, value in rows[3:] if value != str(newest)]
    if rows[2][1] == "MISSING":
        behind.insert(0, "GitHub release")
    if behind:
        print(f"behind {newest}: {', '.join(behind)}")
        return 1
    print(f"everything is at {newest}")
    return 0


def check_tree(
    sites: list[VersionSite],
    notes: NotesHeading,
    newest_tag: Version | None,
    releasing: bool,
) -> list[str]:
    """Every reason the tree is not in a releasable state; empty means OK."""
    if len({s.value for s in sites}) != 1:
        return ["version strings disagree"]
    try:
        tree = Version.parse(sites[0].value)
    except ValueError:
        return [f"not an X.Y.Z version: {sites[0].value!r}"]

    problems: list[str] = []
    if releasing:
        # Release PR: notes are final and the version moves exactly one step.
        if notes.version != str(tree) or notes.is_next_release:
            problems.append(
                f"release_notes.md must open with 'FastLED {tree}' and no "
                "'(Next Release)' marker"
            )
        if newest_tag is not None and tree not in newest_tag.next_steps():
            allowed = ", ".join(str(v) for v in sorted(newest_tag.next_steps()))
            problems.append(
                f"tree is {tree}, newest tag is {newest_tag}; a release must be "
                f"one of {allowed}"
            )
        return problems

    # Steady state: the registry crawler publishes the default branch's version,
    # so it must already be tagged.
    if newest_tag is not None and tree != newest_tag:
        problems.append(
            f"tree is {tree} but the newest tag is {newest_tag}: the registry "
            "crawler would publish this branch as an unreleased version "
            "(use --releasing in a release PR)"
        )
    upcoming = (
        notes.is_next_release and Version.parse(notes.version) in tree.next_steps()
    )
    if notes.version != str(tree) and not upcoming:
        problems.append(
            f"release_notes.md opens with {notes.version}; expected {tree}, or "
            "the next version marked '(Next Release)'"
        )
    return problems


def check_tag(root: Path, tag: str) -> list[str]:
    """Every reason a pushed ``X.Y.Z`` tag disagrees with the tree; empty = OK.

    The tag is what the Arduino index packages; the tree is what the package
    registry's crawler publishes. They must name the same release, and the
    release notes for it must be final -- "(Next Release)" means the notes
    were never closed out. See #4444.
    """
    try:
        version = Version.parse(tag)
    except ValueError:
        return [f"tag {tag!r} is not an X.Y.Z release name"]
    if str(version) != tag:
        # e.g. 3.09.1 parses, but the registries would treat it as a
        # different string than the tree's 3.9.1.
        return [f"tag {tag!r} is not canonical; use {version}"]

    sites = tree_version_sites(root)
    problems = [
        f"{s.path} is {s.value}, tag is {version}" for s in sites if s.value != tag
    ]

    notes = notes_heading(root)
    if notes.is_next_release:
        problems.append(
            f"release_notes.md still marks 'FastLED {notes.version}' as "
            "'(Next Release)'"
        )
    elif notes.version != tag:
        problems.append(
            f"release_notes.md opens with {notes.version}, tag is {version}"
        )
    return problems


def run_release_version_lint(root: Path = PROJECT_ROOT) -> bool:
    """``bash lint`` stage: the tree is either in steady state or a release PR."""
    sites, notes = tree_version_sites(root), notes_heading(root)
    tags = release_tags(root, merged_only=True)
    newest = tags[-1] if tags else None
    steady = check_tree(sites, notes, newest, releasing=False)
    if not steady or not check_tree(sites, notes, newest, releasing=True):
        print("Release version: consistent")
        return True
    for site in sites:
        print(f"  {site.path}: {site.value}")
    print(f"  release_notes.md: {notes.version}")
    for problem in steady:
        print(f"[release-version] {problem}")
    print("Run `bash release check` (or `check --releasing` in a release PR).")
    return False


def cmd_check(root: Path, releasing: bool) -> int:
    sites = tree_version_sites(root)
    notes = notes_heading(root)
    width = max(len(s.path) for s in sites)
    for site in sites:
        print(f"  {site.path:<{width}}  {site.value}")
    marker = " (Next Release)" if notes.is_next_release else ""
    print(f"  {'release_notes.md':<{width}}  {notes.version}{marker}")

    tags = release_tags(root, merged_only=True)
    if not tags:
        print("SKIP tag comparison: no release tags reachable (shallow clone?)")
    problems = check_tree(sites, notes, tags[-1] if tags else None, releasing)
    for problem in problems:
        print(f"FAIL: {problem}")
    if not problems:
        print("OK")
    return 1 if problems else 0


_NOTES_HEADING_RE = re.compile(
    r"^FastLED (\d+\.\d+\.\d+)([^\r\n]*)(\r?\n=+[ \t]*)$", re.MULTILINE
)


def release_notes_section(notes: str, version: Version) -> str | None:
    """The body under ``FastLED <version>`` up to the next version heading."""
    headings = list(_NOTES_HEADING_RE.finditer(notes))
    for i, m in enumerate(headings):
        if m.group(1) == str(version):
            end = headings[i + 1].start() if i + 1 < len(headings) else len(notes)
            return notes[m.end() : end].strip("\r\n").rstrip() or None
    return None


def _sub_once(pattern: str, repl: str, text: str, where: str) -> str:
    out, n = re.subn(pattern, repl, text, count=1, flags=re.MULTILINE)
    if n != 1:
        raise ValueError(f"no version string to rewrite in {where}")
    return out


def _validate_release_notes(notes: str) -> re.Match[str]:
    top = _NOTES_HEADING_RE.search(notes)
    if top is None or "(Next Release)" not in top.group(2):
        raise ValueError(
            "release_notes.md has no '(Next Release)' section on top: write the "
            "notes for this release first"
        )
    if release_notes_section(notes, Version.parse(top.group(1))) is None:
        raise ValueError("the '(Next Release)' section of release_notes.md is empty")
    return top


def _version_edits(root: Path, version: Version) -> list[tuple[Path, str]]:
    notes_path = root / "release_notes.md"
    notes = notes_path.read_text(encoding="utf-8")
    top = _validate_release_notes(notes)

    padded = f"{version.major}.{version.minor:03d}.{version.patch:03d}"
    edits: list[tuple[Path, str]] = []
    path = root / "library.properties"
    edits.append(
        (
            path,
            _sub_once(
                r"^version=.*$",
                f"version={version}",
                path.read_text(encoding="utf-8"),
                path.name,
            ),
        )
    )
    path = root / "library.json"
    edits.append(
        (
            path,
            _sub_once(
                r'^(\s*"version":\s*")[^"]*(")',
                rf"\g<1>{version}\g<2>",
                path.read_text(encoding="utf-8"),
                path.name,
            ),
        )
    )
    path = root / "src" / "FastLED.h"
    header = _sub_once(
        r"^(#define FASTLED_VERSION )\d+$",
        rf"\g<1>{version.as_int()}",
        path.read_text(encoding="utf-8"),
        "src/FastLED.h",
    )
    header, shown_count = re.subn(
        r"(FastLED version )\d+\.\d{3}\.\d{3}", rf"\g<1>{padded}", header
    )
    if shown_count == 0:
        raise ValueError("no displayed version string to rewrite in src/FastLED.h")
    edits.append((path, header))
    path = root / "docs" / "Doxyfile"
    edits.append(
        (
            path,
            _sub_once(
                r"^(PROJECT_NUMBER\s*=\s*)\S+",
                rf"\g<1>{version}",
                path.read_text(encoding="utf-8"),
                "docs/Doxyfile",
            ),
        )
    )
    final_heading = f"FastLED {version}{top.group(3)}"
    edits.append(
        (notes_path, notes[: top.start()] + final_heading + notes[top.end() :])
    )
    return edits


def apply_version(root: Path, version: Version) -> None:
    """Write ``version`` to every site ``tree_version_sites`` reads, and make the
    top release-notes heading final. Raises ValueError, changing nothing, when
    any required rewrite cannot be prepared."""
    edits = _version_edits(root, version)

    # Every rewrite succeeded; only now touch the tree.
    for target, text in edits:
        target.write_text(text, encoding="utf-8", newline="")


def _step(newest: Version, bump: str) -> Version:
    if bump == "major":
        return Version(newest.major + 1, 0, 0)
    if bump == "minor":
        return Version(newest.major, newest.minor + 1, 0)
    return Version(newest.major, newest.minor, newest.patch + 1)


def cmd_prepare(root: Path, bump: str, open_pr: bool) -> int:
    if open_pr:
        if _git(["status", "--porcelain"], root).strip():
            print("FAIL: working tree is not clean; --pr commits only the version bump")
            return 1
        current_branch = _git(["branch", "--show-current"], root).strip()
        if current_branch != "master":
            print("FAIL: --pr must be run from master")
            return 1
        _git(["fetch", "origin", "master", "--tags"], root)
        head = _git(["rev-parse", "HEAD"], root).strip()
        upstream = _git(["rev-parse", "origin/master"], root).strip()
        if head != upstream:
            print("FAIL: local master must match origin/master")
            return 1
    tags = release_tags(root, merged_only=True)
    if not tags:
        print("FAIL: no release tags reachable from HEAD (run: git fetch --tags)")
        return 1
    version = _step(tags[-1], bump)
    branch = f"release/{version}"
    if open_pr and _git(["branch", "--list", branch], root).strip():
        print(f"FAIL: local branch already exists: {branch}")
        return 1
    try:
        _version_edits(root, version)
    except ValueError as e:
        print(f"FAIL: {e}")
        return 1
    if open_pr:
        _git(["checkout", "-b", branch], root)
    apply_version(root, version)
    print(f"prepared {version} (newest tag: {tags[-1]})")
    rc = cmd_check(root, releasing=True)
    if rc != 0 or not open_pr:
        if rc == 0:
            print("review the diff, then open a PR -- or rerun with --pr")
        return rc

    _git(["commit", "-am", f"Rev {version}"], root)
    _git(["push", "-u", "origin", branch], root)
    body = (
        f"Release {version}.\n\n"
        "Merging this PR **is** the release: the `release` workflow tags the merge "
        "commit and creates the GitHub release from the notes below. The tag puts "
        "it in the Arduino index; the package registry's crawler picks the new "
        "version up from `library.json` on master.\n\n"
        f"`bash release check --releasing` passes on this branch."
    )
    result = RunningProcess.run(
        ["gh", "pr", "create", "--title", f"Rev {version}", "--body", body],
        cwd=str(root),
        check=False,
        timeout=120,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    print((result.stdout or "").strip())
    return result.returncode or 0


def cmd_notes(root: Path, version_text: str) -> int:
    notes = (root / "release_notes.md").read_text(encoding="utf-8")
    section = release_notes_section(notes, Version.parse(version_text))
    if section is None:
        print(f"no release notes for {version_text}", file=sys.stderr)
        return 1
    print(section)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="bash release", description=__doc__.split("\n")[0]
    )
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("status", help="what each registry has versus the newest tag")
    chk = sub.add_parser("check", help="version strings are in a releasable state")
    chk.add_argument(
        "--releasing",
        action="store_true",
        help="release PR: the version is one step ahead of the newest tag",
    )
    prep = sub.add_parser("prepare", help="bump every version string for a release")
    prep.add_argument(
        "bump", nargs="?", choices=("patch", "minor", "major"), default="patch"
    )
    prep.add_argument(
        "--pr", action="store_true", help="branch, commit, push and open the PR"
    )
    notes = sub.add_parser("notes", help="print one version's release notes")
    notes.add_argument("version", help="X.Y.Z")
    tag_chk = sub.add_parser(
        "check-tag", help="a pushed X.Y.Z tag agrees with the tree (CI on tag push)"
    )
    tag_chk.add_argument("tag", help="the tag that was pushed")
    args = parser.parse_args(argv)

    if args.command == "status":
        return cmd_status(PROJECT_ROOT)
    if args.command == "prepare":
        return cmd_prepare(PROJECT_ROOT, args.bump, args.pr)
    if args.command == "notes":
        return cmd_notes(PROJECT_ROOT, args.version)
    if args.command == "check-tag":
        problems = check_tag(PROJECT_ROOT, args.tag)
        for problem in problems:
            print(f"FAIL: {problem}")
        if not problems:
            print(f"tag {args.tag} matches the tree")
        return 1 if problems else 0
    return cmd_check(PROJECT_ROOT, args.releasing)


if __name__ == "__main__":
    sys.exit(main())
