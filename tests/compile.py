import os
import subprocess
import json
from pathlib import Path

from ci.paths import PROJECT_ROOT

HERE = Path(__file__).resolve().parent

def run_command(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        print(f"Error executing command: {command}")
        print(stderr.decode())
        exit(1)
    return stdout.decode()

def get_file_mtime(file_path):
    return os.path.getmtime(file_path) if os.path.exists(file_path) else 0

def load_build_cache(cache_file):
    if os.path.exists(cache_file):
        with open(cache_file, 'r') as f:
            return json.load(f)
    return {}

def save_build_cache(cache_file, cache):
    with open(cache_file, 'w') as f:
        json.dump(cache, f)

def needs_recompilation(src_dir, lib_file, cache_file):
    src_files = list(Path(src_dir).rglob("*.cpp")) + list(Path(src_dir).rglob("*.h"))
    lib_mtime = get_file_mtime(lib_file)
    cache = load_build_cache(cache_file)

    for src_file in src_files:
        src_mtime = get_file_mtime(src_file)
        if src_mtime > lib_mtime or str(src_file) not in cache or cache[str(src_file)] < src_mtime:
            return True
    return False

def compile_fastled_library():
    gpp = "uv run python -m ziglang c++"

    # Create .build directory if it doesn't exist
    build_dir = Path(f"{HERE}") / ".build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Set up paths
    src_dir = Path("src")
    lib_name = build_dir / "libfastled.a"
    cache_file = build_dir / "build_cache.json"

    # Check if recompilation is needed
    if not needs_recompilation(src_dir, lib_name, cache_file):
        print("No changes detected. Skipping compilation.")
        return

    # Compile C++ files into object files
    cpp_files = list(src_dir.rglob("*.cpp"))
    obj_files = []

    for cpp_file in cpp_files:
        rel_path = cpp_file.relative_to(src_dir)
        obj_file = build_dir / rel_path.with_suffix(".o")
        obj_file.parent.mkdir(parents=True, exist_ok=True)
        obj_files.append(obj_file)
        run_command(f"{gpp} -c -std=c++14 -g -fstack-protector-all -Wno-unknown-pragmas -Wno-unused-parameter -Wno-sign-compare -D_USE_MATH_DEFINES -I. -I./src {cpp_file} -o {obj_file}")

    # Create static library
    obj_files_str = " ".join(str(f) for f in obj_files)
    run_command(f"ar rcs {lib_name} {obj_files_str}")

    print(f"Static library '{lib_name}' created successfully.")

    # Clean up object files
    for obj_file in obj_files:
        obj_file.unlink()

    print("Object files cleaned up.")

    # Update build cache
    all_files = cpp_files + list(src_dir.rglob("*.h"))
    cache = {str(file): get_file_mtime(file) for file in all_files}
    save_build_cache(cache_file, cache)

def main():
    os.chdir(str(PROJECT_ROOT))
    print(f"Current directory: {Path('.').absolute()}")
    compile_fastled_library()

if __name__ == "__main__":
    main()
