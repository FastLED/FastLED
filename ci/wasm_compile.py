import sys


def _print_panel(title: str, lines: list[str]) -> None:
    """Print a simple box panel without rich (saves 112ms import)."""
    all_lines = [title, ""] + lines
    width = max(len(line) for line in all_lines) + 4
    border = "-" * width
    print(f"\n+{border}+")
    print(f"| {title:<{width - 2}} |")
    print(f"|{' ' * width}|")
    for line in lines:
        print(f"|  {line:<{width - 3}} |")
    print(f"+{border}+\n")


def parse_args() -> tuple:
    import argparse

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
    known_args, unknown_args = parser.parse_known_args()
    if "--build" in unknown_args:
        print("WARNING: --build is no longer supported. It will be ignored.")
        unknown_args.remove("--build")
    if "-b" in unknown_args:
        print("WARNING: -b is no longer supported. It will be ignored.")
        unknown_args.remove("-b")
    return known_args, unknown_args


def main() -> int:
    args, unknown_args = parse_args()

    # Extract example name from sketch_dir (e.g., "examples/Blink" -> "Blink")
    example_name = args.sketch_dir.split("/")[-1]

    # Check if sketch is filtered out for WASM platform
    try:
        from ci.boards import WEBTARGET
        from ci.compiler.board_example_utils import should_skip_example_for_board

        should_skip, reason = should_skip_example_for_board(WEBTARGET, example_name)
        if should_skip:
            print(f"\nWARNING: Example '{example_name}' is filtered for WASM platform")
            print(f"Reason: {reason}\n")
    except FileNotFoundError:
        pass
    except ValueError as e:
        print(f"\nWARNING: Error checking filters for '{example_name}': {e}")
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"Note: Could not verify filter compatibility ({type(e).__name__})")

    # Print what we're going to do
    if args.run:
        _print_panel(
            "FastLED WASM Build Pipeline",
            [
                f"Will:",
                f"  1. Compile {example_name} to WASM",
                f"  2. Launch headless browser demo",
                f"  3. Run for 5 seconds, verify rendering",
                f"  4. Exit automatically",
            ],
        )
    else:
        _print_panel(
            "FastLED WASM Build Pipeline",
            [
                f"Will:",
                f"  * Compile {example_name} to WASM",
                f"  (Use --run to add automated testing)",
            ],
        )

    steps = "2" if args.run else "1"
    print(f"Step 1/{steps}: Compiling WASM...")

    # Output to examples/<name>/fastled_js/fastled.js to match expected location
    from pathlib import Path

    output_dir = Path("examples") / example_name / "fastled_js"
    output_dir.mkdir(parents=True, exist_ok=True)

    output_js = output_dir / "fastled.js"

    # Call wasm_build directly (in-process) to avoid ~180ms Python spawn overhead
    from ci.wasm_build import main as wasm_build_main

    saved_argv = sys.argv
    sys.argv = [
        "ci.wasm_build",
        "--example",
        example_name,
        "-o",
        str(output_js),
    ] + unknown_args
    try:
        compile_result = wasm_build_main()
    finally:
        sys.argv = saved_argv

    if compile_result != 0:
        print("WASM compilation failed")
        return compile_result

    print("WASM compilation successful")

    # Run tests if --run flag is provided
    if args.run:
        import subprocess

        print(f"\nStep 2/2: Running Playwright tests...\n")

        test_cmd = [sys.executable, "-m", "ci.wasm_test", example_name]
        cmd_str = subprocess.list2cmdline(test_cmd)
        print(f"-> {cmd_str}")
        test_result = subprocess.call(test_cmd)

        if test_result != 0:
            print("WASM tests failed")
            return test_result

        print("All tests passed!")

    return 0


if __name__ == "__main__":
    sys.exit(main())
