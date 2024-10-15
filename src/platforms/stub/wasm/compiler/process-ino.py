#!/usr/bin/env python3

import os
import sys
import subprocess
import argparse
import re

def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Process INO file and dump AST.")
    parser.add_argument("input_file", help="Input source file (.cpp)", nargs='?', default="src/wasm.ino.cpp")
    return parser.parse_args()

def run_command(command):
    result = subprocess.run(command, capture_output=True, text=True)
    return result.stdout, result.stderr, result.returncode

def parse_ast_output(ast_output):
    function_pattern = re.compile(r'FunctionDecl.*?line:\d+:\d+\s+(.*?)\s+\'(.*?)\'')
    functions = function_pattern.findall(ast_output)
    return [f"{name} {prototype}" for name, prototype in functions]

def main() -> None:
    # Parse command-line arguments
    args = parse_arguments()

    # Run compile.py --only-copy
    stdout, stderr, returncode = run_command(["python", "compile.py", "--only-copy"])
    if returncode != 0:
        print(f"Error running compile.py: {stderr}", file=sys.stderr)
        sys.exit(returncode)

    # Copy src/wasm.ino to src/wasm.ino.cpp
    stdout, stderr, returncode = run_command(["cp", "src/wasm.ino", "src/wasm.ino.cpp"])
    if returncode != 0:
        print(f"Error copying file: {stderr}", file=sys.stderr)
        sys.exit(returncode)

    # Ensure the file exists
    if not os.path.isfile(args.input_file):
        print(f"Error: File '{args.input_file}' not found!")
        sys.exit(1)

    # Add Emscripten's Clang to PATH
    os.environ['PATH'] = "/emsdk_portable/upstream/bin:" + os.environ['PATH']

    # Print Clang version
    stdout, stderr, returncode = run_command(["clang", "--version"])
    print(stdout)
    if returncode != 0:
        print(f"Error: {stderr}", file=sys.stderr)
        sys.exit(returncode)

    # Dump the AST
    stdout, stderr, returncode = run_command(["clang", "-Xclang", "-ast-dump", "-fsyntax-only", "-nostdinc", args.input_file])
    
    # Parse the AST output, even if there was an error
    function_prototypes = parse_ast_output(stdout)

    # Print warning if there was an error, but continue processing
    if returncode != 0:
        print(f"Warning: Clang reported an error (possibly due to missing stdio.h), but continuing to process output.", file=sys.stderr)
        print(f"Clang error: {stderr}", file=sys.stderr)

    # Read the contents of the input file
    with open(args.input_file, 'r') as f:
        file_contents = f.read()

    # Find the position of setup() function
    setup_pos = file_contents.find('void setup()')
    if setup_pos == -1:
        print("Error: setup() function not found in the file.")
        sys.exit(1)

    # Insert the prototypes before setup()
    prototypes_text = "\n".join(f"{name.split()[1]} {name.split()[0]}();" for name in function_prototypes if not name.startswith('setup') and not name.startswith('loop'))
    new_contents = file_contents[:setup_pos] + prototypes_text + "\n\n" + file_contents[setup_pos:]

    # Write the modified contents back to the file
    with open(args.input_file, 'w') as f:
        f.write(new_contents)

    print(f"Function prototypes have been inserted into {args.input_file}")

    # Write the function prototypes to protos.txt as well
    with open('protos.txt', 'w') as f:
        f.write("Function Prototypes:\n")
        for prototype in function_prototypes:
            f.write(f"{prototype}\n")
    
    print(f"Function prototypes have also been written to protos.txt")

if __name__ == "__main__":
    main()
