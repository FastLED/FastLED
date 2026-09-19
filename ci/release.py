"""FastLED release helper. Run through the wrapper: ``bash release <command>``.

Commands:
  status   What the newest tag, GitHub, the Arduino index and the PlatformIO
           registry each have. Read-only.
  check    The in-tree version strings are in a releasable state. Read-only.

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
import json
import re
import sys
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from running_process import CalledProcessError, RunningProcess


PROJECT_ROOT = Path(__file__).resolve().parent.parent

REGISTRY_URL = "https://api.registry.platformio.org/v3/packages/fastled/library/FastLED"
ARDUINO_INDEX_URL = "https://downloads.arduino.cc/libraries/library_index.json.gz"

_TAG_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")


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


@dataclass(frozen=True)
class NotesHeading:
    version: str  # X.Y.Z, or "missing"
    is_next_release: bool  # heading carries the "(Next Release)" marker


def notes_heading(root: Path) -> NotesHeading:
    """The first ``FastLED X.Y.Z`` heading in release_notes.md."""
    notes = (root / "release_notes.md").read_text(encoding="utf-8")
    m = re.search(r"^FastLED (\d+\.\d+\.\d+)(.*)$", notes, re.MULTILINE)
    if m is None:
        return NotesHeading("missing", False)
    return NotesHeading(m.group(1), "(Next Release)" in m.group(2))


def _git(args: list[str], root: Path) -> str:
    result = RunningProcess.run(
        ["git", *args],
        cwd=str(root),
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
    import gzip  # noqa: PLC0415 - only status needs it

    data = json.loads(gzip.decompress(_fetch(ARDUINO_INDEX_URL)))
    return sorted(
        Version.parse(lib["version"])
        for lib in data["libraries"]
        if lib["name"] == "FastLED" and _TAG_RE.match(lib["version"])
    )


def github_has_release(tag: Version, root: Path) -> bool:
    result = RunningProcess.run(
        ["gh", "release", "view", str(tag), "--json", "tagName"],
        cwd=str(root),
        check=False,
        timeout=60,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    return result.returncode == 0


def cmd_status(root: Path) -> int:
    tags = release_tags(root, merged_only=False)
    if not tags:
        print("no X.Y.Z tags found (shallow clone? run: git fetch --tags)")
        return 1
    newest = tags[-1]
    rows = [
        ("newest tag", str(newest)),
        ("tree (library.properties)", tree_version_sites(root)[0].value),
        (
            "GitHub release for tag",
            "yes" if github_has_release(newest, root) else "MISSING",
        ),
        ("Arduino index", str(arduino_versions()[-1])),
        ("PlatformIO registry", str(registry_versions()[-1])),
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
    args = parser.parse_args(argv)

    if args.command == "status":
        return cmd_status(PROJECT_ROOT)
    return cmd_check(PROJECT_ROOT, args.releasing)


if __name__ == "__main__":
    sys.exit(main())
