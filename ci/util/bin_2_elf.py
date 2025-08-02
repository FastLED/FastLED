#!/usr/bin/env python3
"""
Improved Binary to ELF Converter
Handles platform-specific binary formats for ESP32, Uno, and other platforms.
"""

import json
import subprocess
from pathlib import Path


def _run_command(command: list[str] | str, show_output: bool = False) -> str:
    """
    Run a command using subprocess and capture the output.

    Args:
        command (list or str): Command to run.
        show_output (bool): Print command and its output if True.

    Returns:
        str: Standard output of the command.

    Raises:
        RuntimeError: If the command fails.
    """
    if isinstance(command, str):
        command = [command]

    if show_output:
        print(f"Running command: {' '.join(command)}")

    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        if show_output:
            print(f"Command failed: {' '.join(command)}")
            print(f"Error: {result.stderr}")
        raise RuntimeError(f"Command failed: {' '.join(command)}\n{result.stderr}")

    if show_output and result.stdout:
        print(f"Command output: {result.stdout}")
    return result.stdout


def _detect_platform_from_paths(as_path: Path, ld_path: Path):
    """
    Detect the target platform from toolchain paths.

    Returns:
        str: Platform name (esp32, avr, arm, etc.)
    """
    path_str = str(as_path).lower()

    if "xtensa-esp32" in path_str or "esp32" in path_str:
        return "esp32"
    elif "avr" in path_str:
        return "avr"
    elif "arm" in path_str:
        return "arm"
    else:
        return "unknown"


def _analyze_binary_structure(bin_file: Path, platform: str):
    """
    Analyze binary structure based on platform.

    Args:
        bin_file (Path): Path to binary file
        platform (str): Target platform

    Returns:
        dict: Binary analysis information
    """
    print(f"Analyzing {platform} binary: {bin_file}")

    if not bin_file.exists():
        raise RuntimeError(f"Binary file not found: {bin_file}")

    with open(bin_file, "rb") as f:
        data = f.read()

    analysis: dict[str, str | int | bytes | dict[str, str | int]] = {
        "platform": platform,
        "size": len(data),
        "header": data[:32] if len(data) >= 32 else data,
    }

    if platform == "esp32":
        # ESP32 binary format analysis
        print(f"ESP32 binary size: {len(data)} bytes")
        if len(data) >= 32:
            # ESP32 image header
            magic = data[0]
            segments = data[1]
            flash_mode = data[2]
            flash_size_freq = data[3]
            analysis["esp32"] = {
                "magic": hex(magic),
                "segments": segments,
                "flash_mode": flash_mode,
                "flash_size_freq": flash_size_freq,
            }
            print(f"ESP32 image - Magic: {hex(magic)}, Segments: {segments}")

    elif platform == "avr":
        # AVR/Arduino binary format (Intel HEX)
        print(f"AVR binary size: {len(data)} bytes")
        if bin_file.suffix.lower() == ".hex":
            print("Intel HEX format detected")
            analysis["format"] = "intel_hex"
        else:
            analysis["format"] = "binary"

    # Print hex dump of first 64 bytes for analysis
    print(f"\nFirst 64 bytes of {platform} binary (hex):")
    for i in range(0, min(64, len(data)), 16):
        hex_bytes = " ".join(f"{b:02x}" for b in data[i : i + 16])
        ascii_chars = "".join(
            chr(b) if 32 <= b < 127 else "." for b in data[i : i + 16]
        )
        print(f"{i:08x}: {hex_bytes:<48} {ascii_chars}")

    return analysis


def _generate_platform_linker_script(map_file: Path, platform: str) -> Path:
    """
    Generate a platform-specific linker script.

    Args:
        map_file (Path): Path to the map file.
        platform (str): Target platform.

    Returns:
        Path: Path to the generated linker script.
    """
    print(f"Generating {platform} linker script...")

    if platform == "esp32":
        # ESP32 memory layout
        linker_script_content = """
        MEMORY
        {
            iram0_0_seg : org = 0x40080000, len = 0x20000
            dram0_0_seg : org = 0x3FFB0000, len = 0x50000
            flash_seg   : org = 0x400D0020, len = 0x330000
        }
        
        SECTIONS
        {
            .iram0.text : {
                *(.iram0.literal .iram0.text)
            } > iram0_0_seg
            
            .flash.text : {
                *(.literal .text .literal.* .text.*)
            } > flash_seg
            
            .flash.rodata : {
                *(.rodata .rodata.*)
            } > flash_seg
            
            .dram0.data : {
                *(.data .data.*)
            } > dram0_0_seg
            
            .dram0.bss : {
                *(.bss .bss.*)
            } > dram0_0_seg
        }
        """
    elif platform == "avr":
        # AVR memory layout
        linker_script_content = """
        MEMORY
        {
            text (rx)   : ORIGIN = 0x0000, LENGTH = 0x8000
            data (rw!x) : ORIGIN = 0x800100, LENGTH = 0x800
            eeprom (rw!x) : ORIGIN = 0x810000, LENGTH = 0x400
        }
        
        SECTIONS
        {
            .text : {
                *(.text)
                *(.text.*)
            } > text
            
            .data : {
                *(.data)
                *(.data.*)
            } > data
            
            .bss : {
                *(.bss)
                *(.bss.*)
            } > data
        }
        """
    else:
        # Generic linker script
        linker_script_content = """
        SECTIONS
        {
            .text 0x00000000 : {
                *(.text)
                *(.text.*)
            }
            .data : {
                *(.data)
                *(.data.*)
            }
            .bss : {
                *(.bss)
                *(.bss.*)
            }
        }
        """

    linker_script_path = map_file.with_suffix(f".{platform}.ld")
    linker_script_path.write_text(linker_script_content)
    print(f"Generated {platform} linker script at: {linker_script_path}")
    return linker_script_path


def _create_platform_object_file(as_path: Path, dummy_obj_path: Path, platform: str):
    """
    Create a platform-specific dummy object file.

    Args:
        as_path (Path): Path to the assembler executable.
        dummy_obj_path (Path): Path to the dummy object file to be created.
        platform (str): Target platform.
    """
    print(f"Creating {platform} dummy object file...")

    if platform == "esp32":
        # ESP32/Xtensa assembly
        assembly_code = """
        .section .text
        .global _start
        .type _start, @function
        _start:
            nop
            j _start
        """
    elif platform == "avr":
        # AVR assembly
        assembly_code = """
        .section .text
        .global _start
        _start:
            nop
            rjmp _start
        """
    else:
        # Generic assembly
        assembly_code = """
        .section .text
        .global _start
        _start:
            nop
        """

    asm_file = dummy_obj_path.with_suffix(".s")
    asm_file.write_text(assembly_code)

    command = [str(as_path), "-o", str(dummy_obj_path), str(asm_file)]
    print(f"Creating {platform} dummy object file: {dummy_obj_path}")
    _run_command(command, show_output=True)

    # Clean up assembly file
    if asm_file.exists():
        asm_file.unlink()


def _create_dummy_elf(
    ld_path: Path, linker_script: Path, dummy_obj: Path, output_elf: Path, platform: str
):
    """
    Create a dummy ELF file using the specified linker script and dummy object file.

    Args:
        ld_path (Path): Path to the ld executable.
        linker_script (Path): Path to the linker script.
        dummy_obj (Path): Path to the dummy object file.
        output_elf (Path): Path to the output ELF file.
        platform (str): Target platform.
    """
    print(f"Creating {platform} dummy ELF file...")

    command = [
        str(ld_path),
        str(dummy_obj),
        "-T",
        str(linker_script),
        "-o",
        str(output_elf),
    ]

    print(f"Creating {platform} dummy ELF file: {output_elf}")
    _run_command(command, show_output=True)


def _update_elf_sections(
    objcopy_path: Path, bin_file: Path, elf_file: Path, platform: str
):
    """
    Update the ELF file sections with binary data (platform-specific).

    Args:
        objcopy_path (Path): Path to the objcopy executable.
        bin_file (Path): Path to the binary file.
        elf_file (Path): Path to the ELF file.
        platform (str): Target platform.
    """
    print(f"Updating {platform} ELF sections...")

    # Platform-specific section names
    section_mapping = {"esp32": ".flash.text", "avr": ".text", "arm": ".text"}

    section_name = section_mapping.get(platform, ".text")

    try:
        command = [
            str(objcopy_path),
            "--update-section",
            f"{section_name}={bin_file}",
            str(elf_file),
        ]
        print(
            f"Updating {platform} ELF file '{elf_file}' section '{section_name}' with binary file '{bin_file}'"
        )
        _run_command(command, show_output=True)
        print(f"Successfully updated {section_name} section")
    except RuntimeError as e:
        print(f"Warning: Could not update {section_name} section: {e}")
        # Try alternative section names
        alternative_sections = [".text", ".data", ".rodata"]
        for alt_section in alternative_sections:
            if alt_section != section_name:
                try:
                    command = [
                        str(objcopy_path),
                        "--update-section",
                        f"{alt_section}={bin_file}",
                        str(elf_file),
                    ]
                    print(f"Trying alternative section: {alt_section}")
                    _run_command(command, show_output=True)
                    print(f"Successfully updated {alt_section} section")
                    break
                except RuntimeError:
                    continue


def bin_to_elf(
    bin_file: Path,
    map_file: Path,
    as_path: Path,
    ld_path: Path,
    objcopy_path: Path,
    output_elf: Path,
):
    """
    Convert a binary file to ELF format with platform detection and analysis.

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
    print("=" * 80)
    print("IMPROVED BINARY TO ELF CONVERTER")
    print("=" * 80)

    # Detect platform from toolchain
    platform = _detect_platform_from_paths(as_path, ld_path)
    print(f"Detected platform: {platform}")

    # Analyze binary structure
    binary_analysis = _analyze_binary_structure(bin_file, platform)

    # Generate platform-specific linker script
    linker_script = _generate_platform_linker_script(map_file, platform)

    # Create platform-specific dummy object file
    dummy_obj_path = bin_file.with_name(f"dummy_{platform}.o")
    _create_platform_object_file(as_path, dummy_obj_path, platform)

    # Create a dummy ELF file using the generated linker script
    _create_dummy_elf(ld_path, linker_script, dummy_obj_path, output_elf, platform)

    # Update the ELF sections with binary data
    _update_elf_sections(objcopy_path, bin_file, output_elf, platform)

    # Clean up temporary files
    if dummy_obj_path.exists():
        dummy_obj_path.unlink()
        print(f"Cleaned up dummy object file: {dummy_obj_path}")

    if linker_script.exists():
        linker_script.unlink()
        print(f"Cleaned up linker script: {linker_script}")

    print(f"\n✅ Successfully created {platform} ELF file: {output_elf}")

    # Save analysis results
    analysis_file = output_elf.with_suffix(".analysis.json")
    with open(analysis_file, "w") as f:
        json.dump(binary_analysis, f, indent=2, default=str)
    print(f"📊 Binary analysis saved to: {analysis_file}")

    return output_elf


# For backward compatibility, keep the old function name
def _generate_linker_script(map_file: Path) -> Path:
    """Legacy function for backward compatibility."""
    return _generate_platform_linker_script(map_file, "generic")
