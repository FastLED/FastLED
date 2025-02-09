# A compilation script specific to fastled's docker compiler.
# This script will pull the users code from a mapped directory,
# then do some processing to convert the *.ino files to *.cpp and
# insert certain headers like "Arduino.h" (pointing to a fake implementation).
# After this, the code is compiled, and the output files are copied back
# to the users mapped directory in the fastled_js folder.
# There are a few assumptions for this script:
# 1. The mapped directory will contain only one directory with the users code, this is
#    enforced by the script that sets up the docker container.
# 2. The docker container has installed compiler dependencies in the /js directory.

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import List


@dataclass
class DateLine:
    dt: datetime
    line: str


class BuildMode(Enum):
    DEBUG = "DEBUG"
    QUICK = "QUICK"
    RELEASE = "RELEASE"

    @classmethod
    def from_string(cls, mode_str: str) -> "BuildMode":
        try:
            return cls[mode_str.upper()]
        except KeyError:
            valid_modes = [mode.name for mode in cls]
            raise ValueError(f"BUILD_MODE must be one of {valid_modes}, got {mode_str}")


@dataclass
class SyntaxCheckResult:
    file_path: Path
    is_valid: bool
    message: str


_CHECK_SYNTAX = False
_COMPILER_PATH = "em++"

JS_DIR = Path("/js")
FASTLED_DIR = JS_DIR / "fastled"
FASTLED_SRC = FASTLED_DIR / "src"
FASTLED_SRC_PLATFORMS = FASTLED_SRC / "platforms"
FASTLED_SRC_PLATFORMS_WASM = FASTLED_SRC_PLATFORMS / "wasm"
FASTLED_SRC_PLATFORMS_WASM_COMPILER = FASTLED_SRC_PLATFORMS_WASM / "compiler"


JS_SRC = JS_DIR / "src"

FASTLED_DIR = JS_DIR / "fastled"
FASTLED_SRC_DIR = FASTLED_DIR / "src"
FASTLED_PLATFORMS_DIR = FASTLED_SRC_DIR / "platforms"
FASTLED_WASM_DIR = FASTLED_PLATFORMS_DIR / "wasm"
FASTLED_COMPILER_DIR = FASTLED_WASM_DIR / "compiler"
FASTLED_MODULES_DIR = FASTLED_COMPILER_DIR / "modules"

PIO_BUILD_DIR = JS_DIR / ".pio/build"
ARDUINO_H_SRC = JS_DIR / "Arduino.h"
INDEX_HTML_SRC = FASTLED_COMPILER_DIR / "index.html"
INDEX_CSS_SRC = FASTLED_COMPILER_DIR / "index.css"
INDEX_JS_SRC = FASTLED_COMPILER_DIR / "index.js"


WASM_COMPILER_SETTTINGS = JS_DIR / "wasm_compiler_flags.py"
OUTPUT_FILES = ["fastled.js", "fastled.wasm"]
HEADERS_TO_INSERT = ["#include <Arduino.h>", '#include "platforms/wasm/js.h"']
FILE_EXTENSIONS = [".ino", ".h", ".hpp", ".cpp"]
MAX_COMPILE_ATTEMPTS = 1  # Occasionally the compiler fails for unknown reasons, but disabled because it increases the build time on failure.
FASTLED_OUTPUT_DIR_NAME = "fastled_js"

assert JS_DIR.exists()
assert ARDUINO_H_SRC.exists()
assert INDEX_HTML_SRC.exists()
assert INDEX_CSS_SRC.exists(), f"Index CSS not found at {INDEX_CSS_SRC}"
assert INDEX_JS_SRC.exists()
assert WASM_COMPILER_SETTTINGS.exists()
assert FASTLED_SRC_PLATFORMS_WASM_COMPILER.exists()
assert JS_DIR.exists(), f"JS_DIR does not exist: {JS_DIR}"
assert ARDUINO_H_SRC.exists(), f"ARDUINO_H_SRC does not exist: {ARDUINO_H_SRC}"
assert INDEX_HTML_SRC.exists(), f"INDEX_HTML_SRC does not exist: {INDEX_HTML_SRC}"
assert INDEX_CSS_SRC.exists(), f"INDEX_CSS_SRC does not exist: {INDEX_CSS_SRC}"
assert INDEX_JS_SRC.exists(), f"INDEX_JS_SRC does not exist: {INDEX_JS_SRC}"
assert (
    WASM_COMPILER_SETTTINGS.exists()
), f"WASM_COMPILER_SETTTINGS does not exist: {WASM_COMPILER_SETTTINGS}"


def copy_files(src_dir: Path, js_src: Path) -> None:
    print("Copying files from mapped directory to container...")
    for item in src_dir.iterdir():
        if item.is_dir():
            print(f"Copying directory: {item}")
            shutil.copytree(item, js_src / item.name, dirs_exist_ok=True)
        else:
            print(f"Copying file: {item}")
            shutil.copy2(item, js_src / item.name)


def compile(
    js_dir: Path, build_mode: BuildMode, auto_clean: bool, no_platformio: bool
) -> int:
    print("Starting compilation process...")
    max_attempts = 1
    env = os.environ.copy()
    env["BUILD_MODE"] = build_mode.name
    print(f"Build mode: {build_mode.name}")
    cmd_list: list[str] = []
    if no_platformio:
        # execute build_archive.syh
        cmd_list = [
            "/bin/bash",
            "-c",
            "/js/build_fast.sh",
        ]
    else:
        cmd_list.extend(["pio", "run"])
        if not auto_clean:
            cmd_list.append("--disable-auto-clean")

    def _open_process(cmd_list: list[str] = cmd_list) -> subprocess.Popen:
        out = subprocess.Popen(
            cmd_list,
            cwd=js_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            env=env,
        )
        return out

    output_lines = []
    for attempt in range(1, max_attempts + 1):
        try:
            print(f"Attempting compilation (attempt {attempt}/{max_attempts})...")
            process = _open_process()
            assert process.stdout is not None
            for line in process.stdout:
                processed_line = line.replace("fastled/src", "src")
                timestamped_line = _timestamp_output(processed_line)
                output_lines.append(timestamped_line)
            process.wait()
            relative_output = _make_timestamps_relative("\n".join(output_lines))
            print(relative_output)
            if process.returncode == 0:
                print(f"Compilation successful on attempt {attempt}")
                return 0
            else:
                raise subprocess.CalledProcessError(process.returncode, ["pio", "run"])
        except subprocess.CalledProcessError:
            print(f"Compilation failed on attempt {attempt}")
            if attempt == max_attempts:
                print("Max attempts reached. Compilation failed.")
                return 1
            print("Retrying...")
    return 1


def insert_header(file: Path) -> None:
    print(f"Inserting header in file: {file}")
    with open(file, "r") as f:
        content = f.read()

    # Remove existing includes
    for header in HEADERS_TO_INSERT:
        content = re.sub(
            rf"^.*{re.escape(header)}.*\n", "", content, flags=re.MULTILINE
        )

    # Remove both versions of Arduino.h include
    arduino_pattern = r'^\s*#\s*include\s*[<"]Arduino\.h[>"]\s*.*\n'
    content = re.sub(arduino_pattern, "", content, flags=re.MULTILINE)

    # Add new headers at the beginning
    content = "\n".join(HEADERS_TO_INSERT) + "\n" + content

    with open(file, "w") as f:
        f.write(content)
    print(f"Processed: {file}")


def transform_to_cpp(src_dir: Path) -> None:
    print("Transforming files to cpp...")
    ino_files = list(src_dir.glob("*.ino"))

    if ino_files:
        ino_file = ino_files[0]
        print(f"Found .ino file: {ino_file}")
        main_cpp = src_dir / "main.cpp"
        if main_cpp.exists():
            print("main.cpp already exists, renaming to main2.hpp")
            main_cpp.rename(src_dir / "main2.hpp")

        new_cpp_file = ino_file.with_suffix(".ino.cpp")
        print(f"Renaming {ino_file} to {new_cpp_file.name}")
        ino_file.rename(new_cpp_file)

        if (src_dir / "main2.hpp").exists():
            print(f"Including main2.hpp in {new_cpp_file.name}")
            with open(new_cpp_file, "a") as f:
                f.write('#include "main2.hpp"\n')


def insert_headers(
    src_dir: Path, exclusion_folders: List[Path], file_extensions: List[str]
) -> None:
    print("Inserting headers in source files...")
    for file in src_dir.rglob("*"):
        if (
            file.suffix in file_extensions
            and not any(folder in file.parents for folder in exclusion_folders)
            and file.name != "Arduino.h"
        ):
            insert_header(file)


def process_ino_files(src_dir: Path) -> None:
    transform_to_cpp(src_dir)
    exclusion_folders: List[Path] = []
    insert_headers(src_dir, exclusion_folders, FILE_EXTENSIONS)
    print("Transform to cpp and insert header operations completed.")


def check_syntax_with_gcc(file_path, gcc_path="gcc"):
    """
    Perform syntax checking on a C or C++ source file using GCC.

    Parameters:
        file_path (str): Path to the source file to check.
        gcc_path (str): Path to the GCC executable (default is 'gcc').

    Returns:
        bool: True if syntax is correct, False otherwise.
        str: Output or error message from GCC.
    """
    try:
        # Run GCC with -fsyntax-only flag for syntax checking
        cmd_list = [
            gcc_path,
            "-fsyntax-only",
            "-std=gnu++20",
            "-fpermissive",
            "-Wno-everything",  # Suppress all warnings
            "-I",
            "/js/src/",  # Add /js/src/ to the include path
            "-I",
            "/js/fastled/src/",  # Add /js/fastled/src/ to the include path
            "-I",
            "/emsdk/upstream/emscripten/system/include",
            file_path,
        ]
        cmd_str = subprocess.list2cmdline(cmd_list)
        print(f"Running command: {cmd_str}")
        result = subprocess.run(
            cmd_list,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        # Check the return code to determine if syntax is valid
        if result.returncode == 0:
            return True, "Syntax check passed successfully."
        else:
            return False, result.stderr
    except FileNotFoundError:
        return False, f"GCC not found at {gcc_path}."
    except Exception as e:
        return False, str(e)


def check_syntax(
    directory_path: Path, gcc_path: str = "gcc"
) -> list[SyntaxCheckResult]:
    # os walk
    out: list[SyntaxCheckResult] = []
    exclusion_list = set("fastled_js")
    for root, dirs, files in os.walk(directory_path):
        # if sub directory is in exclusion list, skip
        dirs[:] = [d for d in dirs if d not in exclusion_list]
        for file in files:
            if file.endswith(".cpp") or file.endswith(".ino"):
                file_path = os.path.join(root, file)
                is_valid, message = check_syntax_with_gcc(file_path, gcc_path)
                if not is_valid:
                    print(f"Syntax check failed for file: {file_path}")
                    print(f"Error message: {message}")
                    out.append(
                        SyntaxCheckResult(
                            file_path=Path(file_path), is_valid=False, message=message
                        )
                    )
                else:
                    print(f"Syntax check passed for file: {file_path}")
                    out.append(
                        SyntaxCheckResult(
                            file_path=Path(file_path),
                            is_valid=True,
                            message="Syntax check passed successfully.",
                        )
                    )
    return out


def _make_timestamps_relative(stdout: str) -> str:
    def parse(line: str) -> DateLine:
        parts = line.split(" ")
        if len(parts) < 2:
            raise ValueError(f"Invalid line: {line}")

        date_str, time_str = parts[:2]
        rest = " ".join(parts[2:])
        # Parse with microsecond precision
        dt = datetime.strptime(f"{date_str} {time_str}", "%Y-%m-%d %H:%M:%S.%f")
        return DateLine(dt, rest)

    lines = stdout.split("\n")
    if not lines:
        return stdout
    parsed: list[DateLine] = []
    for line in lines:
        if not line.strip():  # Skip empty lines
            continue
        try:
            parsed.append(parse(line))
        except ValueError:
            print(f"Failed to parse line: {line}")
            continue

    if not parsed:
        return stdout

    outlines: list[str] = []
    start_time = parsed[0].dt

    # Calculate relative times with
    for p in parsed:
        delta = p.dt - start_time
        seconds = delta.total_seconds()
        line_str = f"{seconds:3.2f} {p.line}"
        outlines.append(line_str)

    return "\n".join(outlines)


def _timestamp_output(line: str) -> str:
    now = datetime.now()
    timestamp = now.strftime("%Y-%m-%d %H:%M:%S.%f")
    return f"{timestamp} {line.rstrip()}"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile FastLED for WASM")
    parser.add_argument(
        "--mapped-dir",
        type=Path,
        default="/mapped",
        help="Directory containing source files (default: /mapped)",
    )
    parser.add_argument(
        "--keep-files", action="store_true", help="Keep source files after compilation"
    )
    parser.add_argument(
        "--only-copy",
        action="store_true",
        help="Only copy files from mapped directory to container",
    )
    parser.add_argument(
        "--only-insert-header",
        action="store_true",
        help="Only insert headers in source files",
    )
    parser.add_argument(
        "--only-compile", action="store_true", help="Only compile the project"
    )
    parser.add_argument(
        "--profile",
        action="store_true",
        help="Enable profiling for compilation to see what's taking so long.",
    )

    parser.add_argument(
        "--disable-auto-clean",
        action="store_true",
        help="Massaive speed improvement to not have to rebuild everything, but flakes out sometimes.",
        default=os.getenv("DISABLE_AUTO_CLEAN", "0") == "1",
    )
    parser.add_argument(
        "--no-platformio",
        action="store_true",
        help="Don't use platformio to compile the project, use the new system of direct emcc calls.",
    )
    # Add mutually exclusive build mode group
    build_mode = parser.add_mutually_exclusive_group()
    build_mode.add_argument("--debug", action="store_true", help="Build in debug mode")
    build_mode.add_argument(
        "--quick",
        action="store_true",
        default=True,
        help="Build in quick mode (default)",
    )
    build_mode.add_argument(
        "--release", action="store_true", help="Build in release mode"
    )

    return parser.parse_args()


def find_project_dir(mapped_dir: Path) -> Path:
    mapped_dirs: List[Path] = list(mapped_dir.iterdir())
    if len(mapped_dirs) > 1:
        raise ValueError(
            f"Error: More than one directory found in {mapped_dir}, which are {mapped_dirs}"
        )

    src_dir: Path = mapped_dirs[0]
    return src_dir


def process_compile(
    js_dir: Path, build_mode: BuildMode, auto_clean: bool, no_platformio: bool
) -> None:
    print("Starting compilation...")
    rtn = compile(js_dir, build_mode, auto_clean, no_platformio=no_platformio)
    print(f"Compilation return code: {rtn}")
    if rtn != 0:
        print("Compilation failed.")
        raise RuntimeError("Compilation failed.")
    print("Compilation successful.")


def cleanup(args: argparse.Namespace, js_src: Path) -> None:
    if not args.keep_files and not (args.only_copy or args.only_insert_header):
        print("Removing temporary source files")
        shutil.rmtree(js_src)
    else:
        print("Keeping temporary source files")


def hash_file(file_path: Path) -> str:
    hasher = hashlib.md5()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def main() -> int:
    print("Starting FastLED WASM compilation script...")
    args = parse_args()
    print(f"Keep files flag: {args.keep_files}")
    print(f"Using mapped directory: {args.mapped_dir}")

    if args.profile:
        print("Enabling profiling for compilation.")
        # Profile linking
        os.environ["EMPROFILE"] = "2"

    try:
        if JS_SRC.exists():
            shutil.rmtree(JS_SRC)
        JS_SRC.mkdir(parents=True, exist_ok=True)
        src_dir = find_project_dir(args.mapped_dir)

        any_only_flags = args.only_copy or args.only_insert_header or args.only_compile

        do_copy = not any_only_flags or args.only_copy
        do_insert_header = not any_only_flags or args.only_insert_header
        do_compile = not any_only_flags or args.only_compile

        if do_copy:
            copy_files(src_dir, JS_SRC)
            print("Copying Arduino.h to src/Arduino.h")
            shutil.copy(ARDUINO_H_SRC, JS_SRC / "Arduino.h")
            if args.only_copy:
                return 0

        if do_insert_header:
            process_ino_files(JS_SRC)
            if args.only_insert_header:
                print("Transform to cpp and insert header operations completed.")
                return 0

        if _CHECK_SYNTAX:
            print("Performing syntax check...")
            syntax_results = check_syntax(
                directory_path=JS_SRC, gcc_path=_COMPILER_PATH
            )
            failed_checks = [r for r in syntax_results if not r.is_valid]
            if failed_checks:
                print("\nSyntax check failed!")
                for result in failed_checks:
                    print(f"\nFile: {result.file_path}")
                    print(f"Error: {result.message}")
                return 1
            print("Syntax check passed for all files.")

        no_platformio: bool = args.no_platformio

        if do_compile:
            try:
                # Determine build mode from args
                if args.debug:
                    build_mode = BuildMode.DEBUG
                elif args.release:
                    build_mode = BuildMode.RELEASE
                else:
                    # Default to QUICK mode if neither debug nor release specified
                    build_mode = BuildMode.QUICK

                process_compile(
                    js_dir=JS_DIR,
                    build_mode=build_mode,
                    auto_clean=not args.disable_auto_clean,
                    no_platformio=no_platformio,
                )
            except Exception as e:
                print(f"Error: {str(e)}")
                return 1

            def _get_build_dir_platformio() -> Path:
                build_dirs = [d for d in PIO_BUILD_DIR.iterdir() if d.is_dir()]
                if len(build_dirs) != 1:
                    raise RuntimeError(
                        f"Expected exactly one build directory in {PIO_BUILD_DIR}, found {len(build_dirs)}: {build_dirs}"
                    )
                build_dir: Path = build_dirs[0]
                return build_dir

            def _get_build_dir_cmake() -> Path:
                return JS_DIR / "build"

            if no_platformio:
                build_dir = _get_build_dir_cmake()
            else:
                build_dir = _get_build_dir_platformio()

            print("Copying output files...")
            fastled_js_dir: Path = src_dir / FASTLED_OUTPUT_DIR_NAME
            fastled_js_dir.mkdir(parents=True, exist_ok=True)

            for file in ["fastled.js", "fastled.wasm"]:
                _src = build_dir / file
                _dst = fastled_js_dir / file
                print(f"Copying {_src} to {_dst}")
                shutil.copy2(_src, _dst)

            print(f"Copying {INDEX_HTML_SRC} to output directory")
            shutil.copy2(INDEX_HTML_SRC, fastled_js_dir / "index.html")
            print(f"Copying {INDEX_CSS_SRC} to output directory")
            shutil.copy2(INDEX_CSS_SRC, fastled_js_dir / "index.css")

            # copy all js files in FASTLED_COMPILER_DIR to output directory
            Path(fastled_js_dir / "modules").mkdir(parents=True, exist_ok=True)
            for _file in FASTLED_MODULES_DIR.iterdir():
                if _file.suffix == ".js":
                    print(f"Copying {_file} to output directory")
                    shutil.copy2(_file, fastled_js_dir / "modules" / _file.name)

            fastled_js_mem = build_dir / "fastled.js.mem"
            fastled_wasm_map = build_dir / "fastled.wasm.map"
            fastled_js_symbols = build_dir / "fastled.js.symbols"
            if fastled_js_mem.exists():
                print(f"Copying {fastled_js_mem} to output directory")
                shutil.copy2(fastled_js_mem, fastled_js_dir / fastled_js_mem.name)
            if fastled_wasm_map.exists():
                print(f"Copying {fastled_wasm_map} to output directory")
                shutil.copy2(fastled_wasm_map, fastled_js_dir / fastled_wasm_map.name)
            if fastled_js_symbols.exists():
                print(f"Copying {fastled_js_symbols} to output directory")
                shutil.copy2(
                    fastled_js_symbols, fastled_js_dir / fastled_js_symbols.name
                )
            print("Copying index.js to output directory")
            shutil.copy2(INDEX_JS_SRC, fastled_js_dir / "index.js")
            optional_input_data_dir = src_dir / "data"
            output_data_dir = fastled_js_dir / optional_input_data_dir.name

            # Handle data directory if it exists
            manifest: list[dict] = []
            if optional_input_data_dir.exists():
                # Clean up existing output data directory
                if output_data_dir.exists():
                    for _file in output_data_dir.iterdir():
                        _file.unlink()

                # Create output data directory and copy files
                output_data_dir.mkdir(parents=True, exist_ok=True)
                for _file in optional_input_data_dir.iterdir():
                    if _file.is_file():  # Only copy files, not directories
                        filename: str = _file.name
                        if filename.endswith(".embedded.json"):
                            print("Embedding data file")
                            filename_no_embedded = filename.replace(
                                ".embedded.json", ""
                            )
                            # read json file
                            with open(_file, "r") as f:
                                data = json.load(f)
                            hash_value = data["hash"]
                            size = data["size"]
                            manifest.append(
                                {
                                    "name": filename_no_embedded,
                                    "path": f"data/{filename_no_embedded}",
                                    "size": size,
                                    "hash": hash_value,
                                }
                            )
                        else:
                            print(f"Copying {_file.name} -> {output_data_dir}")
                            shutil.copy2(_file, output_data_dir / _file.name)
                            hash = hash_file(_file)
                            manifest.append(
                                {
                                    "name": _file.name,
                                    "path": f"data/{_file.name}",
                                    "size": _file.stat().st_size,
                                    "hash": hash,
                                }
                            )

            # Write manifest file even if empty
            print("Writing manifest files.json")
            manifest_json_str = json.dumps(manifest, indent=2, sort_keys=True)
            with open(fastled_js_dir / "files.json", "w") as f:
                f.write(manifest_json_str)
        cleanup(args, JS_SRC)

        print("Compilation process completed successfully")
        return 0

    except Exception as e:
        import traceback

        stacktrace = traceback.format_exc()
        print(stacktrace)
        print(f"Error: {str(e)}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
