"""FastLED release helper. Run through the wrapper: ``bash release <command>``.

Commands:
  status          What the newest tag, GitHub, the Arduino index and the
                  PlatformIO registry each have. Read-only.
  check           Every in-tree version string agrees, and the tree is exactly
                  one step ahead of the newest tag. Read-only.
  publish <tag>   Publish a tagged release to the PlatformIO registry. Packs and
                  validates only, unless --yes is given.

This file is the one place the PlatformIO tool may be invoked (CLAUDE.md,
"One exception to the PlatformIO ban"), and only for its registry-publish
subcommand. It is fetched on demand with uvx, so it is not a project dependency.

Publishing needs PLATFORMIO_AUTH_TOKEN: a token from an account that belongs to
the ``fastled`` organization on the registry (``account token`` subcommand of the
tool, after logging in). Without --yes no credentials are needed.
"""

import argparse
import json
import os
import re
import sys
import tarfile
import tempfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from running_process import RunningProcess


PROJECT_ROOT = Path(__file__).resolve().parent.parent

REGISTRY_OWNER = "fastled"
REGISTRY_URL = "https://api.registry.platformio.org/v3/packages/fastled/library/FastLED"
ARDUINO_INDEX_URL = "https://downloads.arduino.cc/libraries/library_index.json.gz"
AUTH_TOKEN_ENV = "PLATFORMIO_AUTH_TOKEN"
# uvx fetches the tool into its own cache; nothing is added to pyproject.toml.
PUBLISH_TOOL = ["uvx", "--from", "platformio", "pio"]

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

    notes = (root / "release_notes.md").read_text(encoding="utf-8")
    m = re.search(r"^FastLED (\d+\.\d+\.\d+)", notes, re.MULTILINE)
    sites.append(VersionSite("release_notes.md", m.group(1) if m else "missing"))
    return sites


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
    tags = [t for t in _git(args, root).split() if _TAG_RE.match(t)]
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


def cmd_check(root: Path) -> int:
    sites = tree_version_sites(root)
    width = max(len(s.path) for s in sites)
    for site in sites:
        print(f"  {site.path:<{width}}  {site.value}")
    ok = True
    if len({s.value for s in sites}) != 1:
        print("FAIL: version strings disagree")
        ok = False
    tags = release_tags(root, merged_only=True)
    if not tags:
        print("SKIP: no release tags reachable from HEAD (shallow clone?)")
    elif ok:
        tree, newest = Version.parse(sites[0].value), tags[-1]
        if tree not in newest.next_steps():
            allowed = ", ".join(str(v) for v in sorted(newest.next_steps()))
            print(
                f"FAIL: tree is {tree}, newest tag is {newest}; must be one of {allowed}"
            )
            ok = False
        else:
            print(f"OK: {tree} is one step ahead of tag {newest}")
    return 0 if ok else 1


def export_tag(tag: Version, root: Path, dest: Path) -> Path:
    """Unpack the tagged tree into ``dest`` -- never publish the working tree."""
    archive = dest / "src.tar"
    # -o writes the tar itself; a captured binary stdout would be corrupted.
    _git(["archive", "--format=tar", "-o", str(archive), str(tag)], root)
    tree = dest / "tree"
    with tarfile.open(archive) as tar:
        tar.extractall(tree, filter="data")
    return tree


def publish_command(package: Path) -> list[str]:
    return [
        *PUBLISH_TOOL,
        "pkg",
        "publish",
        str(package),
        "--owner",
        REGISTRY_OWNER,
        "--type",
        "library",
        "--no-notify",
        "--no-interactive",
    ]


def _run_tool(cmd: list[str], cwd: Path) -> int:
    proc = RunningProcess(cmd, cwd=cwd, auto_run=False, capture=True, encoding="utf-8")
    proc.start()
    proc.wait(echo=True)
    return proc.returncode if proc.returncode is not None else 1


def cmd_publish(root: Path, tag_text: str, yes: bool) -> int:
    tag = Version.parse(tag_text)
    if tag not in release_tags(root, merged_only=False):
        print(f"FAIL: no tag {tag} in this clone")
        return 1
    if tag in registry_versions():
        print(f"{tag} is already in the PlatformIO registry; nothing to do")
        return 0
    if yes and not os.environ.get(AUTH_TOKEN_ENV):
        print(
            f"FAIL: {AUTH_TOKEN_ENV} is not set. It must be a token from an account in "
            f"the '{REGISTRY_OWNER}' registry organization."
        )
        return 1

    with tempfile.TemporaryDirectory(prefix="fastled-release-") as tmp:
        tree = export_tag(tag, root, Path(tmp))
        manifest = json.loads((tree / "library.json").read_text(encoding="utf-8"))
        if manifest.get("version") != str(tag):
            # The registry takes the version from library.json, not from the tag.
            print(
                f"FAIL: tag {tag} ships library.json version "
                f"{manifest.get('version')!r}; the registry would publish it as that"
            )
            return 1

        package = Path(tmp) / f"FastLED-{tag}.tar.gz"
        rc = _run_tool(
            [*PUBLISH_TOOL, "pkg", "pack", str(tree), "-o", str(package)], tree
        )
        if rc != 0 or not package.is_file():
            print("FAIL: packing the library failed")
            return rc or 1
        print(f"packed {package.name}: {package.stat().st_size:,} bytes")

        if not yes:
            print(f"dry run: pass --yes to publish {tag} as '{REGISTRY_OWNER}/FastLED'")
            return 0
        rc = _run_tool(publish_command(package), tree)
        if rc != 0:
            print("FAIL: publish was rejected (see output above)")
            return rc
    print(f"published {tag}; the registry can take a few minutes to list it")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="bash release", description=__doc__.split("\n")[0]
    )
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("status", help="what each registry has versus the newest tag")
    sub.add_parser(
        "check", help="version strings agree and lead the newest tag by one step"
    )
    pub = sub.add_parser("publish", help="publish a tag to the PlatformIO registry")
    pub.add_argument("tag", help="release tag, X.Y.Z")
    pub.add_argument(
        "--yes", action="store_true", help="actually publish (default: pack only)"
    )
    args = parser.parse_args(argv)

    if args.command == "status":
        return cmd_status(PROJECT_ROOT)
    if args.command == "check":
        return cmd_check(PROJECT_ROOT)
    return cmd_publish(PROJECT_ROOT, args.tag, args.yes)


if __name__ == "__main__":
    sys.exit(main())
