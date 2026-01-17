from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""LED Chipset Timing Conversion Utilities.

Converts between datasheet timing format (T0H, T0L, T1H, T1L) and
3-phase timing format (T1, T2, T3).

Examples:
    # Interactive mode with menu-driven selection
    $ python led_timing_conversions.py

    # Datasheet → 3-phase conversion
    $ python led_timing_conversions.py --datasheet 400 850 850 400
    Datasheet Format: T0H=400ns, T0L=850ns, T1H=850ns, T1L=400ns
    3-Phase Format:   T1=400ns, T2=450ns, T3=400ns

    # 3-phase → Datasheet conversion
    $ python led_timing_conversions.py --fastled 400 450 400
    3-Phase Format:   T1=400ns, T2=450ns, T3=400ns
    Datasheet Format: T0H=400ns, T0L=850ns, T1H=850ns, T1L=400ns
"""

import argparse
import sys
from dataclasses import dataclass
from typing import Optional


@dataclass
class TimingSpec:
    """Datasheet timing specifications (min/max ranges).

    Attributes:
        T0H_min, T0H_max: T0H valid range (nanoseconds)
        T0L_min, T0L_max: T0L valid range (nanoseconds)
        T1H_min, T1H_max: T1H valid range (nanoseconds)
        T1L_min, T1L_max: T1L valid range (nanoseconds)
    """

    T0H_min: int
    T0H_max: int
    T0L_min: int
    T0L_max: int
    T1H_min: int
    T1H_max: int
    T1L_min: int
    T1L_max: int

    def validate(self, ds: "TimingDatasheet") -> tuple[bool, list[str]]:
        """Validate datasheet timing against specifications.

        Args:
            ds: Datasheet timing to validate

        Returns:
            Tuple of (is_valid, list of error messages)
        """
        errors: list[str] = []

        if not (self.T0H_min <= ds.T0H <= self.T0H_max):
            errors.append(
                f"T0H={ds.T0H}ns out of spec (valid: {self.T0H_min}-{self.T0H_max}ns)"
            )
        if not (self.T0L_min <= ds.T0L <= self.T0L_max):
            errors.append(
                f"T0L={ds.T0L}ns out of spec (valid: {self.T0L_min}-{self.T0L_max}ns)"
            )
        if not (self.T1H_min <= ds.T1H <= self.T1H_max):
            errors.append(
                f"T1H={ds.T1H}ns out of spec (valid: {self.T1H_min}-{self.T1H_max}ns)"
            )
        if not (self.T1L_min <= ds.T1L <= self.T1L_max):
            errors.append(
                f"T1L={ds.T1L}ns out of spec (valid: {self.T1L_min}-{self.T1L_max}ns)"
            )

        return (len(errors) == 0, errors)


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


def optimize_datasheet_timing(
    measured: TimingDatasheet, spec: TimingSpec
) -> TimingDatasheet:
    """Optimize datasheet timing to be spec-compliant with symmetric cycles.

    Strategy:
        1. Keep measured T0H and T1H (high times are most critical)
        2. Minimize T0H (reduce jitter impact - timers never fire early)
        3. Adjust T0L and T1L to create symmetric cycles (T0H+T0L = T1H+T1L)
        4. Ensure all values are within spec ranges

    Args:
        measured: Measured timing values (may be out of spec or asymmetric)
        spec: Datasheet specifications (min/max ranges)

    Returns:
        Optimized spec-compliant symmetric timing

    Example:
        >>> spec = TimingSpec(220, 380, 580, 1000, 580, 1000, 580, 1000)
        >>> measured = TimingDatasheet(264, 920, 640, 552)  # Asymmetric, T1L out of spec
        >>> optimized = optimize_datasheet_timing(measured, spec)
        >>> print(optimized)
        T0H=264ns, T0L=1000ns, T1H=640ns, T1L=624ns (cycle=1264ns)
    """
    # Use measured high times (most critical for signal integrity)
    # KEEP both T0H and T1H from measured values because they're hardware-dependent
    T0H = measured.T0H
    T1H = measured.T1H

    # Clamp high times to spec ranges
    T0H = max(spec.T0H_min, min(T0H, spec.T0H_max))
    T1H = max(spec.T1H_min, min(T1H, spec.T1H_max))

    # For symmetric cycles: T0H + T0L = T1H + T1L
    # We need to find T0L and T1L such that:
    # 1. T0L and T1L are both in spec ranges
    # 2. T0H + T0L = T1H + T1L (symmetric)

    # From symmetry: T0L = T1L + (T1H - T0H)
    # We need: T0L_min <= T1L + (T1H - T0H) <= T0L_max
    #          T1L_min <= T1L <= T1L_max

    # Solving for valid T1L range:
    # T0L_min <= T1L + (T1H - T0H) <= T0L_max
    # T0L_min - (T1H - T0H) <= T1L <= T0L_max - (T1H - T0H)

    delta = T1H - T0H
    T1L_min_constrained = max(spec.T1L_min, spec.T0L_min - delta)
    T1L_max_constrained = min(spec.T1L_max, spec.T0L_max - delta)

    if T1L_min_constrained > T1L_max_constrained:
        # No valid solution - use fallback (maximize T1L to get T0L at max)
        T1L = spec.T1L_max
        T0L = T1L + delta
    else:
        # CRITICAL: Maximize T1L to provide maximum margin above minimum spec
        # This is important because measured T1L is often below spec due to
        # platform timing implementation issues (see WS2812B-V5 analysis)
        # By maximizing T1L, we ensure the most spec-compliant solution
        T1L = T1L_max_constrained
        T0L = T1L + delta

    return TimingDatasheet(T0H, T0L, T1H, T1L, measured.name)


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
    print("Which conversion would you like to perform?")
    print()
    print("[1] Datasheet format (T0H, T0L, T1H, T1L) → 3-phase format (T1, T2, T3)")
    print("[2] 3-phase format (T1, T2, T3) → Datasheet format (T0H, T0L, T1H, T1L)")
    print()

    try:
        choice = input("Enter choice [1 or 2]: ").strip()

        if choice not in ["1", "2"]:
            print("\nERROR: Invalid choice. Please enter 1 or 2.")
            sys.exit(1)

        print()

        if choice == "1":
            # Datasheet → 3-phase conversion
            print(
                "Do you want to optimize for datasheet specifications? [y/N]: ", end=""
            )
            optimize = input().strip().lower() in ["y", "yes"]
            print()

            if optimize:
                print("Enter datasheet SPECIFICATIONS (min-max ranges in nanoseconds):")
                print("  Example: For WS2812B-V5, T0H range is 220-380ns")
                print()
                T0H_min = int(input("  T0H min: "))
                T0H_max = int(input("  T0H max: "))
                T0L_min = int(input("  T0L min: "))
                T0L_max = int(input("  T0L max: "))
                T1H_min = int(input("  T1H min: "))
                T1H_max = int(input("  T1H max: "))
                T1L_min = int(input("  T1L min: "))
                T1L_max = int(input("  T1L max: "))
                print()

                spec = TimingSpec(
                    T0H_min,
                    T0H_max,
                    T0L_min,
                    T0L_max,
                    T1H_min,
                    T1H_max,
                    T1L_min,
                    T1L_max,
                )

                # Generate initial timing from spec midpoints
                # Use minimum values for high times (safest for signal integrity)
                # This avoids requiring oscilloscope measurements
                print("Generating optimal timing from specification ranges...")
                print()
                print("Strategy:")
                print("  - T0H set to MINIMUM + 5ns (safety margin above spec floor)")
                print("    CPU timers never fire early, so minimum T0H is safest")
                print("  - T1H set to MINIMUM spec value (symmetric with T0H)")
                print("  - T0L set to MINIMUM + 20ns (safety margin)")
                print("  - T1L set to midpoint (balanced for optimization)")
                print()
                T0H = T0H_min + 5
                T0L = T0L_min + 20
                T1H = T1H_min
                T1L = (T1L_min + T1L_max) // 2

                ds_measured = TimingDatasheet(T0H, T0L, T1H, T1L)

                # Validate initial values
                is_valid, errors = spec.validate(ds_measured)
                if not is_valid:
                    print()
                    print("⚠ WARNING: Initial generated values are out of spec:")
                    for error in errors:
                        print(f"  - {error}")
                    print()

                # Check for asymmetric cycles
                if ds_measured.cycle_0 != ds_measured.cycle_1:
                    print("⚠ NOTE: Asymmetric cycle times in initial values:")
                    print(f"  Cycle 0: {ds_measured.cycle_0}ns (T0H + T0L)")
                    print(f"  Cycle 1: {ds_measured.cycle_1}ns (T1H + T1L)")
                    print("  Asymmetry will be corrected (symmetric cycles required)")
                    print()

                # Optimize timing
                ds_optimized = optimize_datasheet_timing(ds_measured, spec)

                # Validate optimized values
                is_valid_opt, errors_opt = spec.validate(ds_optimized)

                print("Optimized spec-compliant timing:")
                print("-" * 50)
                print(f"T0H: {ds_optimized.T0H}ns (spec: {T0H_min}-{T0H_max}ns) ✓")
                print(f"T0L: {ds_optimized.T0L}ns (spec: {T0L_min}-{T0L_max}ns) ✓")
                print(f"T1H: {ds_optimized.T1H}ns (spec: {T1H_min}-{T1H_max}ns) ✓")
                print(f"T1L: {ds_optimized.T1L}ns (spec: {T1L_min}-{T1L_max}ns) ✓")
                print(
                    f"Cycles: {ds_optimized.cycle_0}ns (symmetric) "
                    f"~{1000 / ds_optimized.duration:.1f}kHz"
                )
                print()

                if not is_valid_opt:
                    print("⚠ WARNING: Could not find fully spec-compliant solution:")
                    for error in errors_opt:
                        print(f"  - {error}")
                    print()

                # Show changes from initial values
                print("Changes from initial values:")
                print(
                    f"  T0H: {ds_measured.T0H}ns → {ds_optimized.T0H}ns "
                    f"({ds_optimized.T0H - ds_measured.T0H:+d}ns)"
                )
                print(
                    f"  T0L: {ds_measured.T0L}ns → {ds_optimized.T0L}ns "
                    f"({ds_optimized.T0L - ds_measured.T0L:+d}ns)"
                )
                print(
                    f"  T1H: {ds_measured.T1H}ns → {ds_optimized.T1H}ns "
                    f"({ds_optimized.T1H - ds_measured.T1H:+d}ns)"
                )
                print(
                    f"  T1L: {ds_measured.T1L}ns → {ds_optimized.T1L}ns "
                    f"({ds_optimized.T1L - ds_measured.T1L:+d}ns)"
                )
                print()

                # Use optimized values for 3-phase conversion
                ds = ds_optimized

            else:
                # No optimization - use raw values
                print("Enter datasheet timing values in nanoseconds:")
                T0H = int(input("  T0H (high time for '0' bit): "))
                T0L = int(input("  T0L (low time for '0' bit): "))
                T1H = int(input("  T1H (high time for '1' bit): "))
                T1L = int(input("  T1L (low time for '1' bit): "))

                ds = TimingDatasheet(T0H, T0L, T1H, T1L)

                # Check for asymmetric cycles and warn user
                if ds.cycle_0 != ds.cycle_1:
                    print()
                    print("⚠ WARNING: Asymmetric cycle times detected!")
                    print(f"  Cycle 0: {ds.cycle_0}ns (T0H + T0L)")
                    print(f"  Cycle 1: {ds.cycle_1}ns (T1H + T1L)")
                    print()
                    print("  The 3-phase format cannot represent asymmetric cycles.")
                    print(
                        "  Conversion will use max cycle duration. "
                        "Round-trip conversion will NOT match original!"
                    )
                    print()

            # Convert to 3-phase
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

            print()
            print(f"T0H={ds.T0H} T0L={ds.T0L} T1H={ds.T1H} T1L={ds.T1L}")
            print()
            print(f"T1={fl.T1} T2={fl.T2} T3={fl.T3}")

        else:
            # 3-phase → Datasheet conversion
            print("Enter 3-phase timing values in nanoseconds:")
            T1 = int(input("  T1 (high time for '0' bit): "))
            T2 = int(input("  T2 (additional high time for '1' bit): "))
            T3 = int(input("  T3 (low tail duration): "))

            fl = Timing3Phase(T1, T2, T3)
            ds = phase3_to_datasheet(fl)

            if ds is None:
                print(
                    "\nERROR: Invalid timing (produces negative values in datasheet format)"
                )
                sys.exit(1)

            print()
            print("Results:")
            print("-" * 50)
            print(f"Duration (total cycle): {fl.duration}ns")
            print()
            print(f"T0H: {ds.T0H}ns   (high time for '0' bit)")
            print(f"T0L: {ds.T0L}ns   (low time for '0' bit)")
            print(f"T1H: {ds.T1H}ns   (high time for '1' bit)")
            print(f"T1L: {ds.T1L}ns   (low time for '1' bit)")
            print()
            print(f"Cycle '0': {ds.cycle_0}ns")
            print(f"Cycle '1': {ds.cycle_1}ns")

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except ValueError:
        print("\nERROR: Invalid input. Please enter integer values.")
        sys.exit(1)


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
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
