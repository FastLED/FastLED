import sys
from dataclasses import dataclass


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


@dataclass
class WasmCompileArgs:
    """Parsed command line for the WASM compile entry point."""

    sketch_dir: str
    run: bool
    check: bool
    passthrough_args: list[str]


def parse_args(argv: list[str] | None = None) -> WasmCompileArgs:
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
    parser.add_argument(
        "--check",
        action="store_true",
        help="Compile, then fail if the headless browser reports a runtime error",
    )
    known_args, unknown_args = parser.parse_known_args(argv)
    if "--build" in unknown_args:
        print("WARNING: --build is no longer supported. It will be ignored.")
        unknown_args.remove("--build")
    if "-b" in unknown_args:
        print("WARNING: -b is no longer supported. It will be ignored.")
        unknown_args.remove("-b")
    return WasmCompileArgs(
        sketch_dir=known_args.sketch_dir,
        run=known_args.run,
        check=known_args.check,
        passthrough_args=unknown_args,
    )


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    run_browser_check = args.run or args.check

    # Keep the sketch path relative to examples/ (e.g. "examples/Fx/FxCylon"
    # -> "Fx/FxCylon") so resolution stays unambiguous when two sketches share
    # a basename in different directories. `example_name` is the bare name and
    # is used only for display and board filtering.
    sketch_rel = args.sketch_dir.replace("\\", "/").strip("/")
    if sketch_rel.startswith("examples/"):
        sketch_rel = sketch_rel[len("examples/") :]
    example_name = sketch_rel.split("/")[-1]

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
    if run_browser_check:
        _print_panel(
            "FastLED WASM Build Pipeline",
            [
                "Will:",
                f"  1. Compile {example_name} to WASM",
                "  2. Launch headless browser check",
                "  3. Run for 5 seconds, verify rendering",
                "  4. Exit automatically",
            ],
        )
    else:
        _print_panel(
            "FastLED WASM Build Pipeline",
            [
                "Will:",
                f"  * Compile {example_name} to WASM",
                "  (Use --check to add browser error detection)",
            ],
        )

    steps = "2" if run_browser_check else "1"
    print(f"Step 1/{steps}: Compiling WASM...")

    # Output to examples/<name>/fastled_js/fastled.js to match expected location
    from ci.wasm_build import resolve_example_dir

    output_dir = resolve_example_dir(sketch_rel) / "fastled_js"
    output_dir.mkdir(parents=True, exist_ok=True)

    output_js = output_dir / "fastled.js"

    # Call wasm_build directly (in-process) to avoid ~180ms Python spawn overhead
    from ci.wasm_build import main as wasm_build_main

    saved_argv = sys.argv
    sys.argv = [
        "ci.wasm_build",
        "--example",
        sketch_rel,
        "-o",
        str(output_js),
    ] + args.passthrough_args
    try:
        compile_result = wasm_build_main()
    finally:
        sys.argv = saved_argv

    if compile_result != 0:
        print("WASM compilation failed")
        return compile_result

    print("WASM compilation successful")

    # --check is the CI-friendly spelling; retain --run as its compatibility
    # alias for scripts that predate browser console validation.
    if run_browser_check:
        import subprocess

        print("\nStep 2/2: Running Playwright tests...\n")

        # Pass the examples-relative path, not the bare name: ci.wasm_test
        # serves examples/<arg>/fastled_js, which is only the directory the
        # build wrote to when nested sketches keep their parent segments.
        test_cmd = [sys.executable, "-m", "ci.wasm_test", sketch_rel]
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
