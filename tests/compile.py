import os
import subprocess
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

def compile_fastled_library():
    gpp = "uv run python -m ziglang c++"

    # Create .build directory if it doesn't exist
    build_dir = Path(f"{HERE}") / ".build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Compile C++ files into object files
    src_dir = Path("src")
    cpp_files = list(src_dir.rglob("*.cpp"))
    obj_files = []

    for cpp_file in cpp_files:
        rel_path = cpp_file.relative_to(src_dir)
        obj_file = build_dir / rel_path.with_suffix(".o")
        obj_file.parent.mkdir(parents=True, exist_ok=True)
        obj_files.append(obj_file)
        run_command(f"{gpp} -c -std=c++14 -g -fstack-protector-all -Wno-unknown-pragmas -Wno-unused-parameter -Wno-sign-compare -D_USE_MATH_DEFINES -I. -I./src {cpp_file} -o {obj_file}")

    # Create static library
    lib_name = build_dir / "libfastled.a"
    obj_files_str = " ".join(str(f) for f in obj_files)
    run_command(f"ar rcs {lib_name} {obj_files_str}")

    print(f"Static library '{lib_name}' created successfully.")

    # Clean up object files
    for obj_file in obj_files:
        obj_file.unlink()

    print("Object files cleaned up.")

def main():
    os.chdir(str(PROJECT_ROOT))
    print(f"Current directory: {Path('.').absolute()}")
    compile_fastled_library()

if __name__ == "__main__":
    main()
