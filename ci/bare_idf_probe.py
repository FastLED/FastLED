"""Syntax ratchet for ESP32 without Arduino; this does not test linking."""

import argparse
import json
import re
import shlex
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from running_process import CompletedProcess, RunningProcess

from ci.util.fbuild_runner import get_fbuild_executable


ROOT = Path(__file__).resolve().parents[1]


ALLOWLIST = "ci/bare_idf_allowlist.txt"
SKETCH = """#include <FastLED.h>
CRGB leds[1];
extern "C" void app_main(void) {
    FastLED.addLeds<WS2812, 2, GRB>(leds, 1);
    leds[0] = CRGB::Red;
    FastLED.show();
    FastLED.delay(1);
}
"""


def run(command: list[str], cwd: Path = ROOT) -> CompletedProcess:
    return RunningProcess.run(
        command,
        cwd=str(cwd),
        check=False,
        timeout=900,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )


def bare_flags(arguments: list[str], source: str) -> list[str]:
    """Remove Arduino and output/dependency options, retaining SDK flags."""
    output: list[str] = []
    index = 1
    while index < len(arguments):
        arg = (
            arguments[index].replace(chr(92), "")
            if arguments[index].startswith("-D")
            else arguments[index]
        )
        index += 1
        if arg in ("-o", "-MF", "-MT", "-MQ"):
            index += 1
            continue
        if arg in ("-c", "-MMD", "-MD", "-MP") or arg == source:
            continue
        if arg == "-D":
            define = arguments[index]
            index += 1
            if not define.startswith("ARDUINO"):
                output.extend([arg, define])
            continue
        if arg.startswith("-DARDUINO"):
            continue
        if arg in ("-I", "-isystem", "-iquote"):
            path = arguments[index]
            index += 1
            if not arduino_include(path):
                output.extend([arg, path])
            continue
        if any(
            arg.startswith(prefix) and arduino_include(arg[len(prefix) :])
            for prefix in ("-I", "-isystem", "-iquote")
        ):
            continue
        if arg.startswith("--target="):
            continue  # fbuild's clang database is replayed with its GNU SDK compiler.
        output.append(arg)
    return output + [
        f"-I{ROOT / 'src'}",
        "-DFL_NO_ARDUINO=1",
        "-fsyntax-only",
        "-fmax-errors=1",
    ]


def arduino_include(path: str) -> bool:
    # Include farms can be symlinks into Arduino libraries.
    parts = set(Path(path).resolve().parts)
    return bool(parts & {"cores", "variants", "libraries"})


def entries(text: str) -> set[str]:
    """Repository-relative TUs from an allowlist, ignoring blanks and comments.

    The comment marker is checked on the STRIPPED line: a comment indented by
    whitespace is still a comment, and treating it as an entry would make the
    shrink-only comparison fail on a file that only has comments.
    """
    parsed: set[str] = set()
    for raw in text.splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            parsed.add(line)
    return parsed


def check_allowlist(offenders: set[str], base_ref: str) -> bool:
    allowed = entries((ROOT / ALLOWLIST).read_text())
    exists = run(["git", "cat-file", "-e", f"{base_ref}^{{commit}}"])
    if exists.returncode:
        raise ValueError(f"Cannot resolve allowlist base {base_ref}")
    base = run(["git", "show", f"{base_ref}:{ALLOWLIST}"])
    previous = entries(base.stdout) if base.returncode == 0 else set()
    return compare_allowlist(offenders, allowed, previous)


def compare_allowlist(
    offenders: set[str], allowed: set[str], previous: set[str]
) -> bool:
    """Reject growth, new failures, and resolved entries left in the list."""
    added = allowed - previous
    unexpected = offenders - allowed
    stale = allowed - offenders
    for title, items in (
        ("Allowlist additions forbidden", added),
        ("New offenders", unexpected),
        ("Remove resolved allowlist entries", stale),
    ):
        if items:
            print(f"{title}:\n" + "\n".join(sorted(items)))
    return not (added or unexpected or stale)


def compile_database() -> Path:
    """Stage the board with repository tooling, then ask fbuild for exact flags."""
    stage = ROOT / ".build/fbuild/esp32dev"
    from ci.stage_fbuild_project import stage_fbuild_project

    staged = stage_fbuild_project("esp32dev", "Blink", build_dir=stage)
    if not staged.success:
        print(staged.output)
        raise ValueError("Could not stage esp32dev")
    executable = get_fbuild_executable()
    if executable is None:
        raise ValueError("fbuild executable not found")
    result = run(
        [executable, str(stage), "build", "-e", "esp32dev", "--target", "compiledb"]
    )
    if result.returncode:
        print(result.stdout)
        raise ValueError("fbuild compiledb generation failed")
    databases = sorted((stage / ".fbuild/build").glob("**/compile_commands.json"))
    if len(databases) != 1:
        raise ValueError(
            f"Expected one esp32dev compile database, found {len(databases)}"
        )
    return databases[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiledb", type=Path)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--base-ref", default="origin/master")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    database = args.compiledb or compile_database()
    if database is None:
        raise ValueError("fbuild did not produce compile_commands.json")
    commands = json.loads(database.read_text())
    candidates = [
        entry for entry in commands if entry["file"].endswith((".cpp", ".ino.cpp"))
    ]
    if not candidates:
        raise ValueError("Compile database contains no C++ entries")
    entry = next(
        (item for item in candidates if "/cores/" not in item["file"]), candidates[0]
    )
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    compiler = args.compiler or Path(arguments[0])
    if compiler.name in ("clang++", "clang"):
        # fbuild emits clang-style databases even when using GCC. Resolve the
        # installed esp32 GNU toolchain instead of invoking host clang.
        matches = sorted(
            Path.home().glob(".fbuild/*/cache/toolchains/**/bin/xtensa-esp32-elf-g++")
        )
        if not matches:
            raise ValueError("No ESP32 GNU compiler found; pass --compiler")
        referenced = [
            path
            for path in matches
            if any(str(path.parent.parent) in flag for flag in arguments)
        ]
        selected = referenced or matches
        if len(selected) != 1:
            raise ValueError(
                "Multiple ESP32 GNU toolchains cached; pass --compiler explicitly"
            )
        compiler = selected[0]
    flags = bare_flags(arguments, entry["file"])
    sources = sorted((ROOT / "src").glob("*.cpp")) + sorted(
        (ROOT / "src/fl/build").glob("*.cpp")
    )
    scratch = ROOT / ".build/bare-idf-probe"
    scratch.mkdir(parents=True, exist_ok=True)
    sketch = scratch / "app_main.cpp"
    sketch.write_text(SKETCH)
    sources.append(sketch)

    def probe(source: Path) -> str | None:
        result = run([str(compiler), *flags, str(source)], Path(entry["directory"]))
        if result.returncode == 0:
            return None
        diagnostic = next(
            (
                line
                for line in result.stdout.splitlines()
                if re.search(r"(?:fatal )?error:", line)
            ),
            result.stdout.strip(),
        )
        name = source.relative_to(ROOT).as_posix()
        print(f"FAIL {name}: {diagnostic}", flush=True)
        # Keep locations in diagnostics, but ratchet TU names so harmless line
        # shifts cannot require expanding the allowlist.
        return name

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        offenders = {name for name in pool.map(probe, sources) if name is not None}
    print(
        f"bare-IDF probe: {len(sources) - len(offenders)}/{len(sources)} passed; {len(offenders)} offenders"
    )
    return 0 if check_allowlist(offenders, args.base_ref) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except (ValueError, OSError) as exc:
        print(f"bare-IDF probe infrastructure error: {exc}", file=sys.stderr)
        sys.exit(2)
