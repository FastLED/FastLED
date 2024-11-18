
"""
This module provides functions to generate a hash of all files in a directory.
Source files like ino,cpp,h,hpp are concatenated and preprocessed with GCC.
Data date files are hashed as is.
"""


__all__ = ["generate_hash_of_project_files"]

from dataclasses import dataclass
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import List
import hashlib
import os
import re
import subprocess
import warnings

_SOURCE_EXTENSIONS = [".cpp", ".hpp", ".h", ".ino"]

@dataclass
class ProjectFiles:
    """A class to represent the project files."""

    src_files: list[Path]
    other_files: list[Path]


@dataclass
class SrcFileHashResult:
    hash: str
    stdout: str
    error: bool


def hash_string(s: str) -> str:
    return hashlib.sha256(s.encode()).hexdigest()


def collect_files(directory: Path, src_file_extensions: list[str] | None = None) -> ProjectFiles:
    """Collect files from a directory and separate them into source and other files.

    Args:
        directory (Path): The directory to scan for files.

    Returns:
        ProjectFiles: Object containing lists of source and other files.
    """
    src_file_extensions = src_file_extensions or _SOURCE_EXTENSIONS
    print(f"Collecting files from {directory}")

    src_files: list[Path] = []
    other_files: list[Path] = []

    def is_source_file(filename: str) -> bool:
        return any(filename.endswith(ext) for ext in src_file_extensions)

    for root, _, filenames in os.walk(str(directory)):
        for filename in filenames:
            print(f"Checking file: {filename}")
            file_path = Path(os.path.join(root, filename))

            if is_source_file(filename):
                src_files.append(file_path)
            else:
                other_files.append(file_path)

    return ProjectFiles(src_files=src_files, other_files=other_files)


def concatenate_files(file_list: List[Path], output_file: Path) -> None:
    """Concatenate files into a single output file.

    Args:
        file_list (List[str]): List of file paths to concatenate.
        output_file (str): Path to the output file.
    """
    with open(str(output_file), "w", encoding="utf-8") as outfile:
        for file_path in file_list:
            outfile.write(f"// File: {file_path}\n")
            with open(file_path, "r", encoding="utf-8") as infile:
                outfile.write(infile.read())
                outfile.write("\n\n")


def collapse_spaces_preserve_cstrings(line: str):
    def replace_outside_cstrings(match):
        # This function processes the part outside of C strings
        content = match.group(0)
        if content.startswith('"') or content.startswith("'"):
            return content  # It's inside a C string, keep as is
        else:
            # Collapse spaces outside of C strings
            return " ".join(content.split())

    # Regular expression to match C strings and non-C string parts
    pattern = r'\"(?:\\.|[^\"])*\"|\'.*?\'|[^"\']+'
    processed_line = "".join(
        replace_outside_cstrings(match) for match in re.finditer(pattern, line)
    )
    return processed_line


# return a hash
def preprocess_with_gcc(input_file: Path, output_file: Path) -> None:
    """Preprocess a file with GCC, leaving #include directives intact.

    Args:
        input_file (str): Path to the input file.
        output_file (str): Path to the preprocessed output file.
    """
    # Convert paths to absolute paths
    # input_file = os.path.abspath(str(input_file))
    input_file = input_file.absolute()
    output_file = output_file.absolute()
    temp_input = str(input_file) + ".tmp"

    try:
        # Create modified version of input that comments out includes
        with open(str(input_file), "r") as fin, open(str(temp_input), "w") as fout:
            for line in fin:
                if line.strip().startswith("#include"):
                    fout.write(f"// PRESERVED: {line}")
                else:
                    fout.write(line)

        # Run GCC preprocessor with explicit output path in order to remove
        # comments. This is necessary to ensure that the hash
        # of the preprocessed file is consistent without respect to formatting
        # and whitespace.
        gcc_command: list[str] = [
            "gcc",
            "-E",  # Preprocess only
            "-P",  # No line markers
            "-fdirectives-only",
            "-fpreprocessed",  # Handle preprocessed input
            "-x",
            "c++",  # Explicitly treat input as C++ source
            "-o",
            str(output_file),  # Explicit output file
            temp_input,
        ]

        result = subprocess.run(gcc_command, check=True, capture_output=True, text=True)

        if not os.path.exists(output_file):
            raise FileNotFoundError(
                f"GCC failed to create output file. stderr: {result.stderr}"
            )

        # Restore include lines
        with open(output_file, "r") as f:
            content = f.read()

        content = content.replace("// PRESERVED: #include", "#include")
        out_lines: list[str] = []
        # now preform minification to further strip out horizontal whitespace and // File: comments.
        for line in content.split("\n"):
            # Skip file marker comments and empty lines
            line = line.strip()
            if not line:  # skip empty line
                continue
            if line.startswith(
                "// File:"
            ):  # these change because of the temp file, so need to be removed.
                continue
            # Collapse multiple spaces into single space and strip whitespace
            # line = ' '.join(line.split())
            line = collapse_spaces_preserve_cstrings(line)
            out_lines.append(line)
        # Join with new lines
        content = "\n".join(out_lines)
        with open(output_file, "w") as f:
            f.write(content)

        print(f"Preprocessed file saved to {output_file}")

    except subprocess.CalledProcessError as e:
        print(f"GCC preprocessing failed: {e.stderr}")
        raise
    except Exception as e:
        print(f"Preprocessing error: {str(e)}")
        raise
    finally:
        # Clean up temporary file
        try:
            if os.path.exists(temp_input):
                os.remove(temp_input)
        except:  # noqa: E722
            warnings.warn(f"Failed to remove temporary file: {temp_input}")
            pass


def generate_hash_of_src_files(src_files: list[Path]) -> SrcFileHashResult:
    """Generate a hash of all source files in a directory.

    Args:
        src_files (list[Path]): List of source files to hash.

    Returns:
        SrcFileHashResult: Object containing hash, stdout and error status.
    """
    try:
        with TemporaryDirectory() as temp_dir:
            temp_file = Path(temp_dir) / "concatenated_output.cpp"
            preprocessed_file = Path(temp_dir) / "preprocessed_output.cpp"
            concatenate_files(src_files, Path(temp_file))
            preprocess_with_gcc(temp_file, preprocessed_file)
            contents = preprocessed_file.read_text()

            # strip the last line in it:
            parts = contents.split("\n")
            out_lines: list[str] = []
            for line in parts:
                if "concatenated_output.cpp" not in line:
                    out_lines.append(line)

            contents = "\n".join(out_lines)
            return SrcFileHashResult(
                hash=hash_string(contents),
                stdout="",  # No stdout in success case
                error=False,
            )
    except Exception:
        import traceback

        stack_trace = traceback.format_exc()
        print(stack_trace)
        return SrcFileHashResult(hash="", stdout=stack_trace, error=True)


def generate_hash_of_project_files(root_dir: Path) -> str:
    """Generate a hash of all files in a directory.

    Args:
        root_dir (Path): The root directory to hash.

    Returns:
        str: The hash of all files in the directory.
    """
    project_files = collect_files(root_dir)
    src_result = generate_hash_of_src_files(project_files.src_files)
    if src_result.error:
        raise Exception(f"Error hashing source files: {src_result.stdout}")

    other_files = project_files.other_files
    # for all other files, don't pre-process them, just hash them
    hash_object = hashlib.sha256()
    for file in other_files:
        hash_object.update(file.read_bytes())
    other_files_hash = hash_object.hexdigest()
    return hash_string(src_result.hash + other_files_hash)
