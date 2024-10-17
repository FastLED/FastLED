#!/usr/bin/env python3

# This is experimental code to process an INO file and output
# a C++ version like the arduino IDE does, where all the declarations
# of function prototypes are at the top of the file.
# However, clang ast dump will not correctly identify the integer types
# of the arguments. For examples, uint8_t was being identified as "int".
# This could be because we are using clang 11 from emscripten and this
# might have been fixed in a newer version version of clang. However,
# the amount of work to get this to work is massive so I'm stopping here.

import os
import sys
import subprocess
import argparse
import re
from dataclasses import dataclass

@dataclass
class FunctionPrototype:
    return_type: str
    name: str
    args: list[str]

    def __repr__(self) -> str:
        return f"{self.return_type} {self.name}({', '.join(self.args)})"

def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Process INO file and dump AST.")
    parser.add_argument("input_file", help="Input source file (.cpp)")
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

    def predicate(name: str) -> bool:
        if name.startswith('loop ') or name.startswith('setup '):
            return False
        return True
    

    
    def parse(name: str) -> FunctionPrototype | None:
        if name.startswith('invalid '):
            name = name[len('invalid '):]
        parts = name.split()
        if len(parts) < 2:
            return None
        name, type, *rest = parts
        args = " ".join(rest)
        return FunctionPrototype(type, name, args.split(','))

    
    prototypes = [parse(name) for name in function_prototypes if predicate(name)]
    prototypes = [str(proto) for proto in prototypes if proto]

    # Insert the prototypes before setup()
    prototypes_text = "\n".join(prototypes)
    prototypes_text_cleaned = prototypes_text.replace('\n', '\n')  # This line effectively does nothing, but ensures prototypes_text_cleaned is a string
    new_contents = file_contents[:setup_pos] + prototypes_text_cleaned + "\n\n" + file_contents[setup_pos:]

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
