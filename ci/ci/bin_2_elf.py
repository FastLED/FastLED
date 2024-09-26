import subprocess
from pathlib import Path


def _run_command(command: list, show_output=False):
    """
    Run a command using subprocess and capture the output.

    Args:
        command (list): Command to run.
        show_output (bool): Print command and its output if True.

    Returns:
        str: Standard output of the command.

    Raises:
        RuntimeError: If the command fails.
    """
    if show_output:
        print(f"Running command: {' '.join(command)}")
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(command)}\n{result.stderr}")
    if show_output and result.stdout:
        print(f"Command output: {result.stdout}")
    return result.stdout


def _generate_linker_script(map_file: Path) -> Path:
    """
    Generate a linker script based on map file information.

    Args:
        map_file (Path): Path to the map file.

    Returns:
        Path: Path to the generated linker script.
    """
    linker_script_content = """
    SECTIONS
    {
        .text 0x00000000 :
        {
            *(.text)
        }
        .data :
        {
            *(.data)
        }
        .bss :
        {
            *(.bss)
        }
    }
    """
    linker_script_path = map_file.with_suffix(".ld")
    linker_script_path.write_text(linker_script_content)
    print(f"Generated linker script at: {linker_script_path}")
    return linker_script_path


def _create_dummy_object_file(as_path: Path, dummy_obj_path: Path):
    """
    Create a minimal dummy object file using the specified assembler.

    Args:
        as_path (Path): Path to the assembler executable.
        dummy_obj_path (Path): Path to the dummy object file to be created.
    """
    assembly_code = """
    .section .text
    .global _start
    _start:
        nop
    """
    asm_file = dummy_obj_path.with_suffix(".s")
    asm_file.write_text(assembly_code)

    command = [str(as_path), "-o", str(dummy_obj_path), str(asm_file)]
    print(f"Creating dummy object file: {dummy_obj_path}")
    _run_command(command, show_output=True)
    asm_file.unlink()  # Clean up the temporary assembly file


def _create_dummy_elf(
    ld_path: Path, linker_script: Path, dummy_obj: Path, output_elf: Path
):
    """
    Create a dummy ELF file using the specified linker script and dummy object file.

    Args:
        ld_path (Path): Path to the ld executable.
        linker_script (Path): Path to the linker script.
        dummy_obj (Path): Path to the dummy object file.
        output_elf (Path): Path to the output ELF file.
    """
    command = [
        str(ld_path),
        str(dummy_obj),
        "-T",
        str(linker_script),
        "-o",
        str(output_elf),
    ]
    print(f"Creating dummy ELF file: {output_elf}")
    _run_command(command, show_output=True)


def _update_elf_sections(
    objcopy_path: Path, bin_file: Path, elf_file: Path, section_name: str
):
    """
    Update the ELF file sections with binary data.

    Args:
        objcopy_path (Path): Path to the objcopy executable.
        bin_file (Path): Path to the binary file.
        elf_file (Path): Path to the ELF file.
        section_name (str): Name of the section to update.
    """
    command = [
        str(objcopy_path),
        "--update-section",
        f"{section_name}={bin_file}",
        str(elf_file),
    ]
    print(
        f"Updating ELF file '{elf_file}' section '{section_name}' with binary file '{bin_file}'"
    )
    _run_command(command, show_output=True)


def bin_to_elf(
    bin_file: Path,
    map_file: Path,
    as_path: Path,
    ld_path: Path,
    objcopy_path: Path,
    output_elf: Path,
):
    """
    Convert a binary file to ELF format.

    Args:
        bin_file (Path): Path to the input binary file.
        map_file (Path): Path to the map file.
        as_path (Path): Path to the assembler executable.
        ld_path (Path): Path to the linker executable.
        objcopy_path (Path): Path to the objcopy executable.
        output_elf (Path): Path to the output ELF file.

    Returns:
        Path: Path to the generated ELF file.
    """
    # Generate a linker script based on the map file
    linker_script = _generate_linker_script(map_file)

    # Create a minimal dummy object file
    dummy_obj_path = bin_file.with_name("dummy.o")
    _create_dummy_object_file(as_path, dummy_obj_path)

    # Create a dummy ELF file using the generated linker script
    _create_dummy_elf(ld_path, linker_script, dummy_obj_path, output_elf)

    # Update the ELF sections with binary data
    _update_elf_sections(objcopy_path, bin_file, output_elf, ".text")

    # Clean up dummy object file
    if dummy_obj_path.exists():
        dummy_obj_path.unlink()

    if linker_script.exists():
        linker_script.unlink()

    return output_elf
