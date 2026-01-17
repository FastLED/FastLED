import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from rich.console import Console
from rich.panel import Panel

from ci.boards import NATIVE
from ci.compiler.board_example_utils import should_skip_example_for_board
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


console = Console()


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(description="Compile wasm")
    parser.add_argument(
        "sketch_dir",
        nargs="?",
        default="examples/wasm",
        help="The directory of the sketch to compile",
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="Run Playwright tests after compilation (default is compile-only)",
    )
    # return parser.parse_args()
    known_args, unknown_args = parser.parse_known_args()
    # if -b or --build is in the unknown_args, remove it
    if "--build" in unknown_args:
        print("WARNING: --build is no longer supported. It will be ignored.")
        unknown_args.remove("--build")
    if "-b" in unknown_args:
        print("WARNING: -b is no longer supported. It will be ignored.")
        unknown_args.remove("-b")
    return known_args, unknown_args


def run_command(cmd_list: list[str]) -> int:
    """Run a command and return its exit code."""
    cmd_str = subprocess.list2cmdline(cmd_list)
    console.print(f"[dim]→ {cmd_str}[/dim]")
    rtn = subprocess.call(cmd_list)
    if rtn != 0:
        console.print(
            f"[bold red]ERROR:[/bold red] Command failed with return code {rtn}"
        )
    return rtn


def main() -> int:
    args, unknown_args = parse_args()

    # Extract example name from sketch_dir (e.g., "examples/Blink" -> "Blink")
    example_name = args.sketch_dir.split("/")[-1]

    # Check if sketch is filtered out for native/WASM platform
    try:
        should_skip, reason = should_skip_example_for_board(NATIVE, example_name)
        if should_skip:
            console.print()
            console.print(
                f"[bold yellow]⚠️  WARNING:[/bold yellow] Example '{example_name}' is filtered for native/WASM platform"
            )
            console.print(f"[dim]Reason: {reason}[/dim]")
            console.print()
    except FileNotFoundError:
        # Sketch file not found - skip check, compiler will handle it
        pass
    except ValueError as e:
        # Filter syntax error - warn but continue
        console.print(
            f"[bold yellow]⚠️  WARNING:[/bold yellow] Error checking filters for '{example_name}': {e}"
        )
        handle_keyboard_interrupt_properly()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        # Unexpected error during filter check - log and continue
        console.print(
            f"[dim]Note: Could not verify filter compatibility ({type(e).__name__})[/dim]"
        )

    # Print what we're going to do
    console.print()
    if args.run:
        console.print(
            Panel.fit(
                f"[bold cyan]FastLED WASM Build Pipeline[/bold cyan]\n\n"
                f"[yellow]Will:[/yellow]\n"
                f"  [green]1.[/green] Compile [bold]{example_name}[/bold] to WASM\n"
                f"  [green]2.[/green] Launch headless browser demo\n"
                f"  [green]3.[/green] Run for 5 seconds, verify rendering\n"
                f"  [green]4.[/green] Exit automatically",
                title="[bold magenta]Build + Test[/bold magenta]",
                border_style="cyan",
            )
        )
    else:
        console.print(
            Panel.fit(
                f"[bold cyan]FastLED WASM Build Pipeline[/bold cyan]\n\n"
                f"[yellow]Will:[/yellow]\n"
                f"  [green]✓[/green] Compile [bold]{example_name}[/bold] to WASM\n"
                f"  [dim](Use --run to add automated testing)[/dim]",
                title="[bold magenta]Compile Only[/bold magenta]",
                border_style="cyan",
            )
        )
    console.print()

    # Compile the WASM using native toolchain (clang-tool-chain emscripten)
    console.print(
        f"[bold cyan]Step 1/{'2' if args.run else '1'}:[/bold cyan] Compiling WASM..."
    )

    # Use native compiler instead of Docker-based fastled command
    # Output to examples/<name>/fastled_js/fastled.js to match expected location
    output_dir = Path("examples") / example_name / "fastled_js"
    output_dir.mkdir(parents=True, exist_ok=True)

    output_js = output_dir / "fastled.js"
    compile_cmd = [
        sys.executable,
        "-m",
        "ci.wasm_compile_native",
        "--example",
        example_name,
        "-o",
        str(output_js),
    ] + unknown_args
    compile_result = run_command(compile_cmd)

    if compile_result != 0:
        console.print("[bold red]✗ WASM compilation failed[/bold red]")
        return compile_result

    # Copy template files to output directory
    template_dir = Path("src") / "platforms" / "wasm" / "compiler"

    # Copy individual template files
    template_files = [
        "index.html",
        "index.css",
        "index.js",
        "jsconfig.json",
        "types.d.ts",
        "emscripten.d.ts",
    ]

    for template_file in template_files:
        src = template_dir / template_file
        dst = output_dir / template_file
        if src.exists():
            shutil.copy2(src, dst)
            console.print(f"[dim]Copied {template_file} to output directory[/dim]")
        else:
            console.print(f"[yellow]Warning: Template file not found: {src}[/yellow]")

    # Copy entire modules directory
    modules_src = template_dir / "modules"
    modules_dst = output_dir / "modules"
    if modules_src.exists():
        if modules_dst.exists():
            shutil.rmtree(modules_dst)
        shutil.copytree(modules_src, modules_dst)
        console.print("[dim]Copied modules directory to output[/dim]")
    else:
        console.print(
            f"[yellow]Warning: modules directory not found: {modules_src}[/yellow]"
        )

    # Copy entire vendor directory (Three.js and other third-party libraries)
    vendor_src = template_dir / "vendor"
    vendor_dst = output_dir / "vendor"
    if vendor_src.exists():
        if vendor_dst.exists():
            shutil.rmtree(vendor_dst)
        shutil.copytree(vendor_src, vendor_dst)
        console.print("[dim]Copied vendor directory to output[/dim]")
    else:
        console.print(
            f"[yellow]Warning: vendor directory not found: {vendor_src}[/yellow]"
        )

    # Generate files.json manifest for data files
    # This lists data files (.json, .csv, .txt, .bin, etc.) that the sketch can load
    example_dir = Path("examples") / example_name
    data_extensions = {".json", ".csv", ".txt", ".cfg", ".bin", ".dat", ".mp3", ".wav"}
    data_files: list[dict[str, str | int]] = []

    # Scan example directory for data files (exclude .ino and fastled_js output dir)
    for file_path in example_dir.rglob("*"):
        if (
            file_path.is_file()
            and file_path.suffix.lower() in data_extensions
            and "fastled_js" not in file_path.parts
        ):
            # Get relative path from example directory
            rel_path = file_path.relative_to(example_dir)
            data_files.append({"path": str(rel_path), "size": file_path.stat().st_size})

    # Write files.json to output directory
    files_json_path = output_dir / "files.json"
    with open(files_json_path, "w", encoding="utf-8") as f:
        json.dump(data_files, f, indent=2)

    if data_files:
        console.print(
            f"[dim]Generated files.json with {len(data_files)} data file(s)[/dim]"  # pyright: ignore[reportUnknownArgumentType]
        )
    else:
        console.print("[dim]Generated empty files.json (no data files found)[/dim]")

    console.print("[bold green]✓ WASM compilation successful[/bold green]")

    # Run tests if --run flag is provided
    if args.run:
        console.print()
        console.print("[bold cyan]Step 2/2:[/bold cyan] Running Playwright tests...")
        console.print()

        test_cmd = [sys.executable, "-m", "ci.wasm_test", example_name]
        test_result = run_command(test_cmd)

        if test_result != 0:
            console.print("[bold red]✗ WASM tests failed[/bold red]")
            return test_result

        console.print("[bold green]✓ All tests passed![/bold green]")

    return 0


if __name__ == "__main__":
    sys.exit(main())
