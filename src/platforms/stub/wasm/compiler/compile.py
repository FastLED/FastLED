import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Tuple

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
            subprocess.run(['pio', 'run'], cwd=js_dir, check=True)
            print(f"Compilation successful on attempt {attempt}")
            return 0
        except subprocess.CalledProcessError:
            print(f"Compilation failed on attempt {attempt}")
            if attempt == max_attempts:
                print("Max attempts reached. Compilation failed.")
                return 1
            print("Retrying...")
    return 1

def insert_header(file: Path) -> None:
    print(f"Inserting header in file: {file}")
    with open(file, 'r') as f:
        content = f.read()
    
    content = content.replace('#include "platforms/stub/wasm/js.h"\n', '')
    content = '#include "platforms/stub/wasm/js.h"\n' + content
    
    with open(file, 'w') as f:
        f.write(content)
    print(f"Processed: {file}")

def transform_to_cpp(src_dir: Path) -> None:
    print("Transforming files to cpp...")
    ino_files = list(src_dir.glob('*.ino'))
    
    if ino_files:
        print(f"Found .ino file: {ino_files[0]}")
        main_cpp = src_dir / 'main.cpp'
        if main_cpp.exists():
            print("main.cpp already exists, renaming to main2.hpp")
            main_cpp.rename(src_dir / 'main2.hpp')
        
        print(f"Renaming {ino_files[0]} to generated_main.cpp")
        ino_files[0].rename(src_dir / 'generated_main.cpp')
        
        if (src_dir / 'main2.hpp').exists():
            print("Including main2.hpp in main.cpp")
            with open(src_dir / 'main.cpp', 'a') as f:
                f.write('#include "main2.hpp"\n')

def insert_headers(src_dir: Path, exclusion_folders: List[Path], file_extensions: List[str]) -> None:
    print("Inserting headers in source files...")
    for file in src_dir.rglob('*'):
        if file.suffix in file_extensions and not any(folder in file.parents for folder in exclusion_folders):
            insert_header(file)

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile FastLED for WASM")
    parser.add_argument('--keep-files', action='store_true', help="Keep source files after compilation")
    parser.add_argument('--only-copy', action='store_true', help="Only copy files from mapped directory to container")
    parser.add_argument('--only-insert-header', action='store_true', help="Only insert headers in source files")
    parser.add_argument('--only-compile', action='store_true', help="Only compile the project")
    return parser.parse_args()

def find_project_dir(mapped_dir: Path) -> Path:
    mapped_dirs: List[Path] = list(mapped_dir.iterdir())
    if len(mapped_dirs) > 1:
        raise ValueError(f"Error: More than one directory found in {mapped_dir}")
    
    src_dir: Path = mapped_dirs[0]
    return src_dir


def process_ino_files(src_dir: Path) -> None:
    transform_to_cpp(src_dir)
    exclusion_folders = []
    file_extensions = ['.ino', '.h', '.hpp', '.cpp']
    insert_headers(src_dir, exclusion_folders, file_extensions)
    print("Transform to cpp and insert header operations completed.")

def process_compile(js_dir: Path, src_dir: Path) -> None:
    print("Starting compilation...")
    if compile(js_dir) != 0:
        raise RuntimeError("Compilation failed.")

    print("Compilation successful. Copying output files...")
    fastled_js_dir: Path = src_dir / 'fastled_js'
    fastled_js_dir.mkdir(parents=True, exist_ok=True)

    build_dir: Path = next((js_dir / '.pio/build').iterdir())
    
    for file in ['fastled.js', 'fastled.wasm']:
        print(f"Copying {file} to output directory")
        shutil.copy2(build_dir / file, fastled_js_dir / file)
    
    print("Copying index.html to output directory")
    shutil.copy2(js_dir / 'index.html', fastled_js_dir / 'index.html')

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
        js_dir = Path('/js')
        js_src = js_dir / 'src'
        js_src.mkdir(parents=True, exist_ok=True)
        mapped_dir = Path('/mapped')
        src_dir = find_project_dir(mapped_dir)

        any_only_flags = args.only_copy or args.only_insert_header or args.only_compile

        do_copy = not any_only_flags or args.only_copy
        do_insert_header = not any_only_flags or args.only_insert_header
        do_compile = not any_only_flags or args.only_compile

        if do_copy:
            copy_files(src_dir, js_dir / 'src')
            print("Copying Arduino.h to src/Arduino.h")
            shutil.copy(js_dir / 'Arduino.h', js_src / 'Arduino.h')
            if args.only_copy:
                return 0

        if do_insert_header:
            process_ino_files(js_src)
            if args.only_insert_header:
                print("Transform to cpp and insert header operations completed.")
                return 0

        if do_compile:
            process_compile(js_dir, src_dir)

        cleanup(args, js_src)

        print("Compilation process completed successfully")
        return 0

    except Exception as e:
        print(f"Error: {str(e)}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
