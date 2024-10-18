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
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List

JS_DIR = Path("/js")
MAPPED_DIR = Path("/mapped")
JS_SRC = JS_DIR / "src"
PIO_BUILD_DIR = JS_DIR / ".pio/build"
ARDUINO_H_SRC = JS_DIR / "Arduino.h"
INDEX_HTML_SRC = JS_DIR / "index.html"
OUTPUT_FILES = ["fastled.js", "fastled.wasm"]
HEADERS_TO_INSERT = ['#include "Arduino.h"', '#include "platforms/wasm/js.h"']
FILE_EXTENSIONS = [".ino", ".h", ".hpp", ".cpp"]
MAX_COMPILE_ATTEMPTS = 2
FASTLED_OUTPUT_DIR_NAME = "fastled_js"

assert JS_DIR.exists()
assert MAPPED_DIR.exists()
assert ARDUINO_H_SRC.exists()
assert INDEX_HTML_SRC.exists()


def copy_files(src_dir: Path, js_src: Path) -> None:
    print("Copying files from mapped directory to container...")
    for item in src_dir.iterdir():
        if item.is_dir():
            print(f"Copying directory: {item}")
            shutil.copytree(item, js_src / item.name, dirs_exist_ok=True)
        else:
            print(f"Copying file: {item}")
            shutil.copy2(item, js_src / item.name)


def compile(js_dir: Path) -> int:
    print("Starting compilation process...")
    max_attempts = 2

    for attempt in range(1, max_attempts + 1):
        try:
            print(f"Attempting compilation (attempt {attempt}/{max_attempts})...")
            process = subprocess.Popen(
                ["pio", "run"],
                cwd=js_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
            )
            assert process.stdout is not None
            for line in process.stdout:
                processed_line = line.replace(".pio/libdeps/wasm/FastLED/", "")
                print(processed_line, end="")
            process.wait()
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile FastLED for WASM")
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
    return parser.parse_args()


def find_project_dir(mapped_dir: Path) -> Path:
    mapped_dirs: List[Path] = list(mapped_dir.iterdir())
    if len(mapped_dirs) > 1:
        raise ValueError(f"Error: More than one directory found in {mapped_dir}")

    src_dir: Path = mapped_dirs[0]
    return src_dir


def process_compile(js_dir: Path) -> None:
    print("Starting compilation...")
    if compile(js_dir) != 0:
        raise RuntimeError("Compilation failed.")
    print("Compilation successful.")


def cleanup(args: argparse.Namespace, js_src: Path) -> None:
    if not args.keep_files and not (args.only_copy or args.only_insert_header):
        print("Removing temporary source files")
        shutil.rmtree(js_src)
    else:
        print("Keeping temporary source files")


def main() -> int:
    print("Starting FastLED WASM compilation script...")
    args = parse_args()
    print(f"Keep files flag: {args.keep_files}")

    try:
        JS_SRC.mkdir(parents=True, exist_ok=True)
        src_dir = find_project_dir(MAPPED_DIR)

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

        if do_compile:
            process_compile(JS_DIR)
            build_dirs = [d for d in PIO_BUILD_DIR.iterdir() if d.is_dir()]
            if len(build_dirs) != 1:
                raise RuntimeError(
                    f"Expected exactly one build directory in {PIO_BUILD_DIR}, found {len(build_dirs)}: {build_dirs}"
                )
            build_dir: Path = build_dirs[0]

            print("Copying output files...")
            fastled_js_dir: Path = src_dir / FASTLED_OUTPUT_DIR_NAME
            fastled_js_dir.mkdir(parents=True, exist_ok=True)

            for file in ["fastled.js", "fastled.wasm"]:
                print(f"Copying {file} to output directory")
                shutil.copy2(build_dir / file, fastled_js_dir / file)

            print("Copying index.html to output directory")
            shutil.copy2(INDEX_HTML_SRC, fastled_js_dir / "index.html")

        cleanup(args, JS_SRC)

        print("Compilation process completed successfully")
        return 0

    except Exception as e:
        print(f"Error: {str(e)}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
