#!/usr/bin/env python3
"""LED Chipset Timing Conversion Utilities.

Converts between datasheet timing format (T0H, T0L, T1H, T1L) and
3-phase timing format (T1, T2, T3).

Example:
    $ python led_timing_conversions.py --datasheet 400 850 850 400
    Datasheet Format: T0H=400ns, T0L=850ns, T1H=850ns, T1L=400ns
    3-Phase Format:   T1=400ns, T2=450ns, T3=400ns
"""

import argparse
import sys
from dataclasses import dataclass
from typing import List, Optional, Tuple


class TimingDatasheet:
    """LED timing in datasheet format (standard LED specification).

    Attributes:
        T0H: Time HIGH when sending a '0' bit (nanoseconds)
        T0L: Time LOW when sending a '0' bit (nanoseconds)
        T1H: Time HIGH when sending a '1' bit (nanoseconds)
        T1L: Time LOW when sending a '1' bit (nanoseconds)
        name: Human-readable chipset name
    """

    def __init__(self, T0H: int, T0L: int, T1H: int, T1L: int, name: str = "") -> None:
        """Initialize TimingDatasheet."""
        self.T0H = T0H
        self.T0L = T0L
        self.T1H = T1H
        self.T1L = T1L
        self.name = name

    @property
    def cycle_0(self) -> int:
        """Total cycle time for '0' bit (T0H + T0L)."""
        return self.T0H + self.T0L

    @property
    def cycle_1(self) -> int:
        """Total cycle time for '1' bit (T1H + T1L)."""
        return self.T1H + self.T1L

    @property
    def duration(self) -> int:
        """Maximum cycle duration (max of cycle_0 and cycle_1)."""
        return max(self.cycle_0, self.cycle_1)

    def __repr__(self) -> str:
        """Return detailed representation."""
        if self.name:
            return (
                f"TimingDatasheet({self.name}: "
                f"T0H={self.T0H}ns, T0L={self.T0L}ns, "
                f"T1H={self.T1H}ns, T1L={self.T1L}ns)"
            )
        return (
            f"TimingDatasheet(T0H={self.T0H}ns, T0L={self.T0L}ns, "
            f"T1H={self.T1H}ns, T1L={self.T1L}ns)"
        )

    def __str__(self) -> str:
        """Return human-readable string."""
        return (
            f"T0H={self.T0H}ns, T0L={self.T0L}ns, "
            f"T1H={self.T1H}ns, T1L={self.T1L}ns (cycle={self.duration}ns)"
        )


class Timing3Phase:
    """LED timing in 3-phase format (three-phase protocol).

    Protocol Timing:
        At T=0        : Line goes HIGH (start of bit)
        At T=T1       : Line goes LOW (for '0' bit)
        At T=T1+T2    : Line goes LOW (for '1' bit)
        At T=T1+T2+T3 : Cycle complete (ready for next bit)

    Attributes:
        T1: High time for '0' bit (nanoseconds)
        T2: Additional high time for '1' bit (nanoseconds)
        T3: Low tail duration (nanoseconds)
        name: Human-readable chipset name
    """

    def __init__(self, T1: int, T2: int, T3: int, name: str = "") -> None:
        """Initialize Timing3Phase."""
        self.T1 = T1
        self.T2 = T2
        self.T3 = T3
        self.name = name

    @property
    def duration(self) -> int:
        """Total cycle duration (T1 + T2 + T3)."""
        return self.T1 + self.T2 + self.T3

    @property
    def high_time_0(self) -> int:
        """High time for '0' bit (T1)."""
        return self.T1

    @property
    def high_time_1(self) -> int:
        """High time for '1' bit (T1 + T2)."""
        return self.T1 + self.T2

    def __repr__(self) -> str:
        """Return detailed representation."""
        if self.name:
            return (
                f"Timing3Phase({self.name}: "
                f"T1={self.T1}ns, T2={self.T2}ns, T3={self.T3}ns)"
            )
        return f"Timing3Phase(T1={self.T1}ns, T2={self.T2}ns, T3={self.T3}ns)"

    def __str__(self) -> str:
        """Return human-readable string."""
        return (
            f"T1={self.T1}ns, T2={self.T2}ns, T3={self.T3}ns (cycle={self.duration}ns)"
        )


def datasheet_to_phase3(ds: TimingDatasheet) -> Timing3Phase:
    """Convert LED timing from datasheet format to 3-phase format.

    This is the CORRECTED algorithm that fixes bugs in the original
    chipsets.h embedded Python script.

    Algorithm:
        duration = max(T0H + T0L, T1H + T1L)
        T1 = T0H              # High time for '0' bit
        T2 = T1H - T0H        # Additional time for '1' bit
        T3 = duration - T1H   # Tail time after '1' bit drops

    Args:
        ds: LED timing in datasheet format

    Returns:
        LED timing in 3-phase format

    Example:
        >>> ds = TimingDatasheet(T0H=400, T0L=850, T1H=850, T1L=400, name="WS2812B")
        >>> fl = datasheet_to_phase3(ds)
        >>> print(fl)
        T1=400ns, T2=450ns, T3=400ns (cycle=1250ns)
    """
    duration = max(ds.T0H + ds.T0L, ds.T1H + ds.T1L)

    T1 = ds.T0H  # High time for '0' bit
    T2 = ds.T1H - ds.T0H  # Additional time for '1' bit
    T3 = duration - ds.T1H  # Tail time after '1' bit

    return Timing3Phase(T1, T2, T3, ds.name)


def phase3_to_datasheet(
    fl: Timing3Phase, duration: Optional[int] = None
) -> Optional[TimingDatasheet]:
    """Convert LED timing from 3-phase format to datasheet format.

    This conversion is UNDERDETERMINED because:
        - We have 3 input values (T1, T2, T3)
        - We need 4 output values (T0H, T0L, T1H, T1L)

    Assumption: Symmetric cycle times (T0H+T0L = T1H+T1L = duration)
    This gives us: T0L = duration - T0H, T1L = duration - T1H

    Args:
        fl: LED timing in 3-phase format
        duration: Optional total cycle duration (if None, computed as T1+T2+T3)

    Returns:
        LED timing in datasheet format, or None if conversion fails

    Example:
        >>> fl = Timing3Phase(T1=250, T2=625, T3=375, name="WS2812")
        >>> ds = phase3_to_datasheet(fl)
        >>> print(ds)
        T0H=250ns, T0L=1000ns, T1H=875ns, T1L=375ns (cycle=1250ns)
    """
    if duration is None:
        duration = fl.T1 + fl.T2 + fl.T3

    T0H = fl.T1
    T1H = fl.T1 + fl.T2
    T0L = duration - T0H
    T1L = duration - T1H

    # Validate results
    if T0L < 0 or T1L < 0:
        return None  # Invalid timing

    return TimingDatasheet(T0H, T0L, T1H, T1L, fl.name)


def print_phase3_cpp_definition(fl: Timing3Phase, name: str = "CUSTOM") -> None:
    """Print 3-phase C++ struct definition.

    Args:
        fl: 3-phase timing to output
        name: Name for the timing struct
    """
    print("\n3-Phase C++ Definition:")
    print("-" * 50)
    print(f"struct TIMING_{name.upper()} {{")
    print("    enum : uint32_t {")
    print(f"        T1 = {fl.T1},")
    print(f"        T2 = {fl.T2},")
    print(f"        T3 = {fl.T3},")
    print("        RESET = 0")
    print("    };")
    print("};")


def handle_interactive() -> None:
    """Run interactive mode for timing conversion."""
    print("LED Chipset Timing Conversion Tool")
    print("=" * 50)
    print()
    print("Enter the values of T0H, T0L, T1H, T1L in nanoseconds:")

    try:
        T0H = int(input("  T0H: "))
        T0L = int(input("  T0L: "))
        T1H = int(input("  T1H: "))
        T1L = int(input("  T1L: "))
    except KeyboardInterrupt:
        raise
    except ValueError:
        print("\nERROR: Invalid input. Please enter integer values.")
        sys.exit(1)

    ds = TimingDatasheet(T0H, T0L, T1H, T1L)
    fl = datasheet_to_phase3(ds)

    print()
    print("Results:")
    print("-" * 50)
    print(f"Duration (max cycle): {ds.duration}ns")
    print()
    print(f"T1: {fl.T1}ns   (high time for '0' bit)")
    print(f"T2: {fl.T2}ns   (additional time for '1' bit)")
    print(f"T3: {fl.T3}ns   (tail time)")

    print_phase3_cpp_definition(fl)


def handle_datasheet(T0H: int, T0L: int, T1H: int, T1L: int, verbose: bool) -> None:
    """Handle datasheet to 3-phase conversion.

    Args:
        T0H: Time HIGH for '0' bit (nanoseconds)
        T0L: Time LOW for '0' bit (nanoseconds)
        T1H: Time HIGH for '1' bit (nanoseconds)
        T1L: Time LOW for '1' bit (nanoseconds)
        verbose: Whether to show verbose output
    """
    ds = TimingDatasheet(T0H, T0L, T1H, T1L)
    fl = datasheet_to_phase3(ds)

    if verbose:
        print(f"Datasheet Format: {ds}")
        print(f"3-Phase Format:   {fl}")
        print_phase3_cpp_definition(fl)
    else:
        print(f"T1={fl.T1} T2={fl.T2} T3={fl.T3}")


def handle_fastled(T1: int, T2: int, T3: int, verbose: bool) -> None:
    """Handle 3-phase to datasheet conversion.

    Args:
        T1: High time for '0' bit (nanoseconds)
        T2: Additional high time for '1' bit (nanoseconds)
        T3: Low tail duration (nanoseconds)
        verbose: Whether to show verbose output
    """
    fl = Timing3Phase(T1, T2, T3)
    ds = phase3_to_datasheet(fl)

    if ds is None:
        print("ERROR: Invalid timing (negative values in datasheet format)")
        sys.exit(1)

    if verbose:
        print(f"3-Phase Format:   {fl}")
        print(f"Datasheet Format: {ds}")
    else:
        print(f"T0H={ds.T0H} T0L={ds.T0L} T1H={ds.T1H} T1L={ds.T1L}")


@dataclass
class Args:
    """Parsed and resolved command-line arguments."""

    interactive: bool
    datasheet: Optional[tuple[int, int, int, int]]
    fastled: Optional[tuple[int, int, int]]
    verbose: bool


def create_argument_parser() -> argparse.ArgumentParser:
    """Create and configure argument parser.

    Returns:
        Configured ArgumentParser instance
    """
    parser = argparse.ArgumentParser(
        description="LED Chipset Timing Conversion Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode (default if no arguments provided)
  python led_timing_conversions.py
  python led_timing_conversions.py --interactive

  # Convert from datasheet format to 3-phase format
  python led_timing_conversions.py --datasheet 400 850 850 400

  # Convert from 3-phase format to datasheet format
  python led_timing_conversions.py --fastled 250 625 375

  # Verbose output with C++ definition
  python led_timing_conversions.py --datasheet 400 850 850 400 --verbose
""",
    )

    # Create mutually exclusive group for operation modes
    mode_group = parser.add_mutually_exclusive_group(required=False)

    mode_group.add_argument(
        "-i",
        "--interactive",
        action="store_true",
        help="Run in interactive mode (default if no mode specified)",
    )

    mode_group.add_argument(
        "-d",
        "--datasheet",
        nargs=4,
        type=int,
        metavar=("T0H", "T0L", "T1H", "T1L"),
        help="Convert from datasheet format (T0H T0L T1H T1L in nanoseconds)",
    )

    mode_group.add_argument(
        "-f",
        "--fastled",
        nargs=3,
        type=int,
        metavar=("T1", "T2", "T3"),
        help="Convert from 3-phase format (T1 T2 T3 in nanoseconds)",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Show verbose output with detailed explanations and C++ definitions",
    )

    return parser


def parse_arguments(argv: Optional[list[str]] = None) -> Args:
    """Parse command-line arguments and return typed Args dataclass.

    Args:
        argv: Command-line arguments (None = sys.argv)

    Returns:
        Parsed Args dataclass with resolved values
    """
    parser = create_argument_parser()
    args = parser.parse_args(argv)

    # Default to interactive mode if no mode specified
    interactive = args.interactive or not any([args.datasheet, args.fastled])

    # Convert lists to tuples for immutability
    datasheet = tuple(args.datasheet) if args.datasheet else None
    fastled = tuple(args.fastled) if args.fastled else None

    return Args(
        interactive=interactive,
        datasheet=datasheet,
        fastled=fastled,
        verbose=args.verbose,
    )


def main() -> int:
    """Main entry point for CLI.

    Returns:
        Exit code (0 for success, 1 for error)
    """
    try:
        args = parse_arguments()

        if args.interactive:
            handle_interactive()
        elif args.datasheet:
            T0H, T0L, T1H, T1L = args.datasheet
            handle_datasheet(T0H, T0L, T1H, T1L, args.verbose)
        elif args.fastled:
            T1, T2, T3 = args.fastled
            handle_fastled(T1, T2, T3, args.verbose)

        return 0

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
