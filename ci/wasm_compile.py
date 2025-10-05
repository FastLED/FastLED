import argparse
import subprocess
import sys
from typing import List, Tuple

from rich.console import Console
from rich.panel import Panel


console = Console()


def parse_args() -> Tuple[argparse.Namespace, list[str]]:
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


def run_command(cmd_list: List[str]) -> int:
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

    # Compile the WASM (always use --just-compile to avoid opening browser)
    console.print(
        f"[bold cyan]Step 1/{'2' if args.run else '1'}:[/bold cyan] Compiling WASM..."
    )
    compile_cmd = ["fastled", args.sketch_dir, "--just-compile"] + unknown_args
    compile_result = run_command(compile_cmd)

    if compile_result != 0:
        console.print("[bold red]✗ WASM compilation failed[/bold red]")
        return compile_result

    console.print("[bold green]✓ WASM compilation successful[/bold green]")

    # Run tests if --run flag is provided
    if args.run:
        console.print()
        console.print(f"[bold cyan]Step 2/2:[/bold cyan] Running Playwright tests...")
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
