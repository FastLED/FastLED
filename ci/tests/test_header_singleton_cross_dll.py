"""Header-inline function-local-static singletons duplicate per DLL on Windows.

An inline ``static T& instance() { static T s; return s; }`` defined in a
header is instantiated separately in every DLL that includes it, so each
test DLL sees its own copy (FastLED#4641: the rmt5 rollback counter read 0).
Use ``fl::SingletonShared<T>::instance()`` instead.

``*.cpp.hpp`` files are compiled once into fastled and are out of scope.
"""

import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SRC_ROOT = REPO_ROOT / "src"

SINGLETON_RE = re.compile(
    r"static\s+[\w:<>]+\s*&\s*instance\s*\(\s*\)[^{;]*\{\s*static\s+"
)

# Pre-existing offenders. This list must only shrink; never add
# rmt5_support_stubs.h (the #4641 regression) or new entries.
ALLOWLIST: set[str] = {
    # Only in the small-memory branch: a stateless no-op event hub, so a
    # per-DLL copy has no observable state.
    "src/fl/channels/channel_events.h",
    # Host timer-thread manager, pulled in via platforms/isr.h. Not yet
    # confirmed safe across DLLs; migrate to fl::SingletonShared.
    "src/platforms/stub/isr_stub.hpp",
}


def find_offenders(text: str) -> list[int]:
    """Return 1-based line numbers of header-inline static singletons."""
    return [text.count("\n", 0, m.start()) + 1 for m in SINGLETON_RE.finditer(text)]


def _header_files() -> list[Path]:
    files: list[Path] = []
    for pattern in ("*.h", "*.hpp"):
        for path in SRC_ROOT.rglob(pattern):
            if path.name.endswith(".cpp.hpp"):
                continue
            files.append(path)
    return sorted(files)


def _rel(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def test_regex_self_check() -> None:
    positive = (
        "static Foo& instance() FL_NO_EXCEPT {\n    static Foo s;\n    return s;\n}\n"
    )
    negative = (
        "static Foo& instance() FL_NO_EXCEPT {\n"
        "    return fl::SingletonShared<Foo>::instance();\n"
        "}\n"
        "static Foo& instance();\n"
    )
    assert find_offenders(positive) == [1]
    assert find_offenders(negative) == []


def test_no_header_inline_static_singletons() -> None:
    offenders: list[str] = []
    for path in _header_files():
        rel = _rel(path)
        if rel in ALLOWLIST:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        offenders.extend(f"{rel}:{line}" for line in find_offenders(text))
    assert not offenders, (
        "Header-inline function-local static singletons duplicate per DLL on "
        "Windows (FastLED#4641); use fl::SingletonShared<T>::instance():\n  "
        + "\n  ".join(offenders)
    )


def test_allowlist_is_current() -> None:
    assert "src/platforms/shared/mock/esp/32/drivers/rmt5_support_stubs.h" not in (
        ALLOWLIST
    )
    stale: list[str] = []
    for rel in sorted(ALLOWLIST):
        path = REPO_ROOT / rel
        if not path.is_file():
            stale.append(f"{rel} (missing)")
            continue
        if not find_offenders(path.read_text(encoding="utf-8", errors="replace")):
            stale.append(f"{rel} (no longer matches)")
    assert not stale, "Remove stale ALLOWLIST entries:\n  " + "\n  ".join(stale)
