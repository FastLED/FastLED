from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Wrapper script for PCH compilation that fixes dependency file generation
and implements input-hash caching to avoid unnecessary mass rebuilds.

Problem 1: When compiling a PCH with `-MD -MF`, the compiler generates a depfile
that references a temporary .obj file in the Zig cache, not the PCH output file.
This causes Ninja to ignore the depfile and always rebuild the PCH.

Solution 1: After compilation, rewrite the depfile to reference the actual PCH
output file instead of the temporary object file.

Problem 2: Rebuilding the PCH triggers recompilation of ALL test DLLs (246+),
even when the PCH content is semantically unchanged (e.g., only a comment or
whitespace changed in a header). Clang PCH output is non-deterministic (embeds
timestamps), so output-hash comparison doesn't work.

Solution 2: Hash all input files listed in the saved depfile from the previous
build. If the input hash matches, skip compilation entirely. The ninja rule uses
`restat = 1`, so unchanged output triggers no downstream rebuilds. The depfile
is saved as `.d.cache` since ninja deletes the original after reading it.

Usage:
    python compile_pch.py <compiler> <args...>

The script expects:
    - '-o' flag followed by the PCH output path
    - '-MF' flag followed by the depfile path
"""

import hashlib
import shutil
import subprocess
import sys
from pathlib import Path


def _find_depfile_separator(content: str) -> int:
    """Find the index of the target:dependency separator colon in depfile content.

    On Windows, paths contain drive-letter colons (e.g. ``C:\\path``).  A drive
    letter colon is a single alpha character immediately before the colon, at
    the start of a path token (position 0, or preceded by a non-alphanumeric
    such as a space).  We skip those and return the first ``": "`` that is NOT
    a drive-letter colon.

    Returns the index of the ``':'`` character, or ``-1`` if not found.
    """
    idx = 0
    while True:
        space_idx = content.find(": ", idx)
        if space_idx == -1:
            return -1

        # A Windows drive letter looks like  X:  where X is a single alpha
        # character at the start of a token (pos 0 or preceded by whitespace /
        # non-alphanumeric).
        if space_idx > 0:
            char_before = content[space_idx - 1]
            if char_before.isalpha() and (
                space_idx == 1 or not content[space_idx - 2].isalnum()
            ):
                # Looks like a drive letter — skip it.
                idx = space_idx + 1
                continue

        return space_idx


def fix_depfile(depfile_path: Path, pch_output_path: Path) -> None:
    r"""
    Rewrite the depfile to reference the PCH output instead of temporary obj file.

    The compiler-generated depfile has format:
        C:\path\to\temp.obj: \
          dep1.h \
          dep2.h

    We need to change it to:
        tests/output.pch: \
          dep1.h \
          dep2.h

    Args:
        depfile_path: Path to the .d dependency file
        pch_output_path: Path to the .pch output file
    """
    if not depfile_path.exists():
        return

    content = depfile_path.read_text(encoding="utf-8")
    separator_idx = _find_depfile_separator(content)

    if separator_idx == -1:
        return

    # Everything after the separator (including ": ") is the dependencies
    dependencies = content[separator_idx:]

    # Create new depfile with correct target
    # Use forward slashes for Ninja compatibility on Windows
    pch_output_str = str(pch_output_path).replace("\\", "/")
    new_content = f"{pch_output_str}{dependencies}"

    depfile_path.write_text(new_content, encoding="utf-8")


def _tokenize_depfile_deps(deps_str: str) -> list[str]:
    r"""Tokenize the dependency portion of a Make-format depfile.

    Handles:
    - ``\<newline>`` line continuations (removed)
    - ``\ `` escaped spaces within paths (preserved as literal space)
    - Regular whitespace as token delimiters
    """
    # Remove line continuations first
    deps_str = deps_str.replace("\\\r\n", " ").replace("\\\n", " ")

    tokens: list[str] = []
    current: list[str] = []
    i = 0
    while i < len(deps_str):
        ch = deps_str[i]
        if ch == "\\" and i + 1 < len(deps_str) and deps_str[i + 1] == " ":
            # Escaped space — part of the current path token
            current.append(" ")
            i += 2
        elif ch in (" ", "\t", "\n", "\r"):
            # Unescaped whitespace — token delimiter
            if current:
                tokens.append("".join(current))
                current = []
            i += 1
        else:
            current.append(ch)
            i += 1
    if current:
        tokens.append("".join(current))
    return tokens


def parse_depfile_inputs(depfile_path: Path) -> list[Path]:
    """Parse the depfile and return a list of dependency file paths."""
    if not depfile_path.exists():
        return []

    content = depfile_path.read_text(encoding="utf-8")
    separator_idx = _find_depfile_separator(content)

    if separator_idx == -1:
        return []

    # Extract dependencies (everything after ": ")
    deps_str = content[separator_idx + 2 :]

    return [Path(t) for t in _tokenize_depfile_deps(deps_str) if t]


def hash_input_files(files: list[Path]) -> str:
    """Hash the content of all input files to detect changes.

    Always includes each file path in the hash so that different sets of
    missing files produce distinct digests (avoiding false cache hits).
    """
    h = hashlib.sha256()
    for f in sorted(str(p) for p in files):
        p = Path(f)
        h.update(f.encode("utf-8"))  # Always include path in hash
        if p.exists():
            h.update(b"\x01")  # sentinel: file exists
            with open(p, "rb") as fh:
                for chunk in iter(lambda: fh.read(65536), b""):
                    h.update(chunk)
        else:
            h.update(b"\x00")  # sentinel: file missing
    return h.hexdigest()


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: compile_pch.py <compiler> <args...>", file=sys.stderr)
        return 1

    # Parse command line arguments to find -o and -MF flags
    args = sys.argv[1:]
    pch_output: Path | None = None
    depfile: Path | None = None

    i = 0
    while i < len(args):
        if args[i] == "-o" and i + 1 < len(args):
            pch_output = Path(args[i + 1])
            i += 2
        elif args[i] == "-MF" and i + 1 < len(args):
            depfile = Path(args[i + 1])
            i += 2
        else:
            i += 1

    # Input-hash caching: skip compilation if all input files are unchanged.
    # Ninja deletes the depfile after reading it (deps = gcc), so we use a
    # saved copy (.d.cache) from the previous successful build.
    saved_depfile = Path(str(depfile) + ".cache") if depfile else None
    hash_file = Path(str(pch_output) + ".input_hash") if pch_output else None
    if pch_output and saved_depfile and hash_file and pch_output.exists():
        if saved_depfile.exists() and hash_file.exists():
            try:
                input_files = parse_depfile_inputs(saved_depfile)
                if input_files:
                    current_hash = hash_input_files(input_files)
                    stored_hash = hash_file.read_text(encoding="utf-8").strip()
                    if current_hash == stored_hash:
                        print(
                            "PCH inputs unchanged (hash match) - skipping compilation",
                            file=sys.stderr,
                        )
                        return 0
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print(f"WARNING: PCH input hash check failed: {e}", file=sys.stderr)
                # Continue with normal compilation

    # Run the compiler
    result = subprocess.run(args, check=False)

    # Fix the depfile and save caching data if compilation succeeded
    if result.returncode == 0 and pch_output and depfile:
        try:
            fix_depfile(depfile, pch_output)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"ERROR fixing PCH depfile: {e}", file=sys.stderr)
            import traceback

            traceback.print_exc(file=sys.stderr)

        # Save depfile copy and input hash for next build's cache check
        if saved_depfile and hash_file and depfile.exists():
            try:
                shutil.copy2(str(depfile), str(saved_depfile))
                input_files = parse_depfile_inputs(saved_depfile)
                if input_files:
                    current_hash = hash_input_files(input_files)
                    hash_file.write_text(current_hash, encoding="utf-8")
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass  # Non-critical: cache will be rebuilt next time

    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
