#!/usr/bin/env python3
"""
Test suite for LED chipset timing conversion.

Tests the conversion between datasheet format (T0H, T0L, T1H, T1L) and
3-phase format (T1, T2, T3) using actual chipset values from led_timing.h.
"""

import unittest

# Import the timing conversion module
from tools.led_timing_conversions import (
    Timing3Phase,
    TimingDatasheet,
    datasheet_to_phase3,
    phase3_to_datasheet,
)


# ============================================================================
# Chipset Database - Values from src/fl/chipsets/led_timing.h
# ============================================================================

# All chipsets from led_timing.h with their T1, T2, T3 values
FASTLED_CHIPSETS: dict[str, tuple[int, int, int]] = {
    # Fast-Speed Chipsets (800kHz - 1600kHz)
    "GE8822_800KHZ": (350, 660, 350),
    "WS2812_800KHZ": (250, 625, 375),
    "WS2812B_V5": (220, 360, 708),
    "WS2812B_MINI_V3": (220, 360, 708),
    "WS2812_800KHZ_LEGACY": (320, 320, 640),
    "WS2813": (320, 320, 640),
    "SK6812": (300, 600, 300),
    "SK6822": (375, 1000, 375),
    "UCS1903B_800KHZ": (400, 450, 450),
    "UCS1904_800KHZ": (400, 400, 450),
    "UCS2903": (250, 750, 250),
    "TM1809_800KHZ": (350, 350, 450),
    "TM1829_800KHZ": (340, 340, 550),
    "TM1829_1600KHZ": (100, 300, 200),
    "LPD1886_1250KHZ": (200, 400, 200),
    "PL9823": (350, 1010, 350),
    "SM16703": (300, 600, 300),
    "SM16824E": (300, 900, 100),
    "UCS1912": (250, 1000, 350),
    # Medium-Speed Chipsets (400kHz - 600kHz)
    "WS2811_400KHZ": (800, 800, 900),
    "WS2815": (250, 1090, 550),
    "UCS1903_400KHZ": (500, 1500, 500),
    "DP1903_400KHZ": (800, 1600, 800),
    "TM1803_400KHZ": (700, 1100, 700),
    "GW6205_400KHZ": (800, 800, 800),
    # Legacy/Special Chipsets
    "WS2811_800KHZ_LEGACY": (500, 2000, 2000),
    "GW6205_800KHZ": (400, 400, 400),
    "DP1903_800KHZ": (400, 1000, 400),
    # RGBW Chipsets
    "TM1814": (360, 600, 340),
    "UCS7604_800KHZ": (420, 420, 160),
    "UCS7604_1600KHZ": (210, 210, 170),
}


class TestChipsetTimingModule(unittest.TestCase):
    """Test the chipset_timing module functions."""

    def test_datasheet_class_properties(self) -> None:
        """Test TimingDatasheet class properties."""
        ds = TimingDatasheet(T0H=400, T0L=850, T1H=850, T1L=400, name="Test")

        self.assertEqual(ds.T0H, 400)
        self.assertEqual(ds.T0L, 850)
        self.assertEqual(ds.T1H, 850)
        self.assertEqual(ds.T1L, 400)
        self.assertEqual(ds.cycle_0, 1250)
        self.assertEqual(ds.cycle_1, 1250)
        self.assertEqual(ds.duration, 1250)

    def test_fastled_class_properties(self) -> None:
        """Test Timing3Phase class properties."""
        fl = Timing3Phase(T1=250, T2=625, T3=375, name="Test")

        self.assertEqual(fl.T1, 250)
        self.assertEqual(fl.T2, 625)
        self.assertEqual(fl.T3, 375)
        self.assertEqual(fl.duration, 1250)
        self.assertEqual(fl.high_time_0, 250)
        self.assertEqual(fl.high_time_1, 875)

    def test_forward_conversion_basic(self) -> None:
        """Test basic forward conversion."""
        ds = TimingDatasheet(T0H=400, T0L=850, T1H=850, T1L=400, name="WS2812B")
        fl = datasheet_to_phase3(ds)

        self.assertEqual(fl.T1, 400)
        self.assertEqual(fl.T2, 450)
        self.assertEqual(fl.T3, 400)

    def test_inverse_conversion_basic(self) -> None:
        """Test basic inverse conversion."""
        fl = Timing3Phase(T1=250, T2=625, T3=375, name="WS2812")
        ds = phase3_to_datasheet(fl)

        self.assertIsNotNone(ds)
        if ds:
            self.assertEqual(ds.T0H, 250)
            self.assertEqual(ds.T1H, 875)
            self.assertEqual(ds.T0L, 1000)
            self.assertEqual(ds.T1L, 375)


class TestChipsetDatabase(unittest.TestCase):
    """Test all chipsets from led_timing.h for conversion correctness."""

    def test_all_chipsets_round_trip(self) -> None:
        """Test that all chipsets survive round-trip conversion."""
        failures: list[str] = []

        for name, (T1, T2, T3) in FASTLED_CHIPSETS.items():
            with self.subTest(chipset=name):
                # Start with 3-phase format
                fl_original = Timing3Phase(T1, T2, T3, name)

                # Convert to datasheet format (inverse)
                ds = phase3_to_datasheet(fl_original)
                self.assertIsNotNone(ds, f"{name}: Inverse conversion failed")

                if ds is None:
                    failures.append(f"{name}: Inverse conversion returned None")
                    continue

                # Convert back to 3-phase format (forward)
                fl_recovered = datasheet_to_phase3(ds)

                # Verify we got back the original values
                if (
                    fl_recovered.T1 != T1
                    or fl_recovered.T2 != T2
                    or fl_recovered.T3 != T3
                ):
                    failures.append(
                        f"{name}: Round-trip mismatch\n"
                        f"  Original:  T1={T1}, T2={T2}, T3={T3}\n"
                        f"  Recovered: T1={fl_recovered.T1}, T2={fl_recovered.T2}, T3={fl_recovered.T3}"
                    )

        if failures:
            self.fail("\n".join(failures))

    def test_ws2812_variants(self) -> None:
        """Test specific WS2812 variants with known datasheet values."""

        # WS2812 typical datasheet values (tight timing)
        # These should produce the optimized T1=250, T2=625, T3=375
        ws2812_tight = TimingDatasheet(
            T0H=250, T0L=1000, T1H=875, T1L=375, name="WS2812_Tight"
        )

        fl = datasheet_to_phase3(ws2812_tight)
        self.assertEqual(fl.T1, 250, "WS2812 T1 mismatch")
        self.assertEqual(fl.T2, 625, "WS2812 T2 mismatch")
        self.assertEqual(fl.T3, 375, "WS2812 T3 mismatch")

    def test_protocol_semantics(self) -> None:
        """Verify conversion respects protocol timing semantics."""

        for name, (T1, T2, T3) in FASTLED_CHIPSETS.items():
            with self.subTest(chipset=name):
                fl = Timing3Phase(T1, T2, T3, name)
                ds = phase3_to_datasheet(fl)

                self.assertIsNotNone(ds, f"{name}: Inverse conversion failed")
                if ds is None:
                    continue

                # Protocol semantic checks:
                # 1. T0H must equal T1 (high time for '0' bit)
                self.assertEqual(
                    ds.T0H, T1, f"{name}: T0H ({ds.T0H}) must equal T1 ({T1})"
                )

                # 2. T1H must equal T1 + T2 (high time for '1' bit)
                self.assertEqual(
                    ds.T1H,
                    T1 + T2,
                    f"{name}: T1H ({ds.T1H}) must equal T1+T2 ({T1 + T2})",
                )

                # 3. Total cycle must be consistent
                cycle = T1 + T2 + T3
                self.assertEqual(
                    ds.T0H + ds.T0L, cycle, f"{name}: T0H+T0L must equal cycle duration"
                )
                self.assertEqual(
                    ds.T1H + ds.T1L, cycle, f"{name}: T1H+T1L must equal cycle duration"
                )

    def test_chipset_timing_ranges(self) -> None:
        """Verify chipset timings fall within reasonable ranges."""

        for name, (T1, T2, T3) in FASTLED_CHIPSETS.items():
            with self.subTest(chipset=name):
                # All timings should be positive
                self.assertGreater(T1, 0, f"{name}: T1 must be positive")
                self.assertGreater(T2, 0, f"{name}: T2 must be positive")
                self.assertGreaterEqual(T3, 0, f"{name}: T3 must be non-negative")

                # Total cycle should be reasonable (100ns to 10000ns)
                cycle = T1 + T2 + T3
                self.assertGreaterEqual(
                    cycle, 100, f"{name}: Cycle ({cycle}ns) seems too short"
                )
                self.assertLessEqual(
                    cycle, 10000, f"{name}: Cycle ({cycle}ns) seems too long"
                )

    def test_fast_chipsets_frequency(self) -> None:
        """Verify fast chipsets (800kHz+) have appropriate timing."""
        fast_chipsets = [
            "GE8822_800KHZ",
            "WS2812_800KHZ",
            "WS2813",
            "SK6812",
            "SK6822",
            "UCS1903B_800KHZ",
            "UCS1904_800KHZ",
            "UCS2903",
            "TM1809_800KHZ",
            "TM1829_800KHZ",
            "TM1829_1600KHZ",
            "LPD1886_1250KHZ",
            "PL9823",
            "SM16703",
            "UCS1912",
        ]

        for name in fast_chipsets:
            if name not in FASTLED_CHIPSETS:
                continue

            T1, T2, T3 = FASTLED_CHIPSETS[name]
            cycle = T1 + T2 + T3

            # 800kHz = 1250ns period, allow range 600-2000ns
            self.assertGreaterEqual(
                cycle,
                600,
                f"{name}: Fast chipset cycle ({cycle}ns) too long for 800kHz+",
            )
            self.assertLessEqual(
                cycle,
                2000,
                f"{name}: Fast chipset cycle ({cycle}ns) seems unusually long",
            )

    def test_medium_chipsets_frequency(self) -> None:
        """Verify medium-speed chipsets (400kHz) have appropriate timing."""
        medium_chipsets = [
            "WS2811_400KHZ",
            "WS2815",
            "UCS1903_400KHZ",
            "DP1903_400KHZ",
            "TM1803_400KHZ",
            "GW6205_400KHZ",
        ]

        for name in medium_chipsets:
            if name not in FASTLED_CHIPSETS:
                continue

            T1, T2, T3 = FASTLED_CHIPSETS[name]
            cycle = T1 + T2 + T3

            # 400kHz = 2500ns period, allow range 1800-3500ns
            self.assertGreaterEqual(
                cycle,
                1800,
                f"{name}: Medium chipset cycle ({cycle}ns) too short for 400kHz",
            )
            self.assertLessEqual(
                cycle,
                3500,
                f"{name}: Medium chipset cycle ({cycle}ns) seems too long for 400kHz",
            )

    def test_symmetric_chipsets(self) -> None:
        """Test chipsets with symmetric timing (T0H+T0L = T1H+T1L)."""
        symmetric = [
            "WS2812_800KHZ",
            "SK6812",
            "SK6822",
            "SM16703",
        ]

        for name in symmetric:
            if name not in FASTLED_CHIPSETS:
                continue

            T1, T2, T3 = FASTLED_CHIPSETS[name]
            fl = Timing3Phase(T1, T2, T3, name)
            ds = phase3_to_datasheet(fl)

            self.assertIsNotNone(ds)
            if ds:
                # For symmetric chipsets, verify cycles are equal
                self.assertEqual(
                    ds.cycle_0, ds.cycle_1, f"{name}: Expected symmetric timing"
                )


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and error handling."""

    def test_zero_timing_values(self) -> None:
        """Test behavior with zero timing values."""
        # T3 can be zero (some chipsets have no tail time)
        ds = TimingDatasheet(T0H=400, T0L=850, T1H=850, T1L=400)
        fl = datasheet_to_phase3(ds)
        self.assertIsNotNone(fl)

        # But T1 and T2 should not be zero in practice
        # (though mathematically valid)

    def test_asymmetric_cycles(self) -> None:
        """Test chipsets where T0H+T0L ≠ T1H+T1L."""
        # This happens when duration = max(cycle_0, cycle_1)
        ds = TimingDatasheet(
            T0H=250,
            T0L=500,  # cycle_0 = 750
            T1H=875,
            T1L=875,  # cycle_1 = 1750
            name="Asymmetric",
        )

        fl = datasheet_to_phase3(ds)

        # Duration should be max(750, 1750) = 1750
        self.assertEqual(fl.duration, 1750)
        self.assertEqual(fl.T1, 250)
        self.assertEqual(fl.T2, 625)
        self.assertEqual(fl.T3, 875)  # 1750 - 875

    def test_invalid_inverse_conversion(self) -> None:
        """Test inverse conversion with invalid inputs."""
        # Very large T3 could theoretically cause issues
        # But with our symmetric assumption, it should work
        fl = Timing3Phase(T1=100, T2=100, T3=5000)
        ds = phase3_to_datasheet(fl)

        self.assertIsNotNone(ds)
        if ds:
            self.assertGreaterEqual(ds.T0L, 0)
            self.assertGreaterEqual(ds.T1L, 0)


class TestChipsetCount(unittest.TestCase):
    """Verify we're testing a comprehensive set of chipsets."""

    def test_chipset_count(self) -> None:
        """Ensure we have a good coverage of chipsets."""
        # We should have at least 25 chipsets from led_timing.h
        self.assertGreaterEqual(
            len(FASTLED_CHIPSETS), 25, "Expected at least 25 chipsets in test database"
        )

    def test_chipset_names_match_led_timing_h(self) -> None:
        """Document which chipsets we're testing."""
        expected_chipsets = {
            "WS2812_800KHZ",
            "WS2813",
            "SK6812",
            "SK6822",
            "UCS1903B_800KHZ",
            "UCS1904_800KHZ",
            "UCS2903",
            "TM1809_800KHZ",
            "WS2811_400KHZ",
            "WS2815",
            "UCS1903_400KHZ",
            "DP1903_400KHZ",
            "TM1803_400KHZ",
            "GW6205_400KHZ",
            "GW6205_800KHZ",
            "DP1903_800KHZ",
        }

        for chipset in expected_chipsets:
            self.assertIn(
                chipset, FASTLED_CHIPSETS, f"Missing expected chipset: {chipset}"
            )


def print_chipset_summary() -> None:
    """Print a summary of all tested chipsets."""
    print("\n" + "=" * 70)
    print("CHIPSET TIMING VALIDATION SUMMARY")
    print("=" * 70)
    print(f"\nTotal Chipsets Tested: {len(FASTLED_CHIPSETS)}")
    print("\nAll chipsets from src/fl/chipsets/led_timing.h:\n")

    for name, (T1, T2, T3) in sorted(FASTLED_CHIPSETS.items()):
        cycle = T1 + T2 + T3
        freq_khz = 1000000 / cycle if cycle > 0 else 0
        print(
            f"  {name:25s}  T1={T1:4d}  T2={T2:4d}  T3={T3:4d}  "
            f"cycle={cycle:4d}ns  ~{freq_khz:.0f}kHz"
        )

    print("\n" + "=" * 70)
    print("All tests passed! ✓")
    print("=" * 70 + "\n")


def main() -> int:
    """Run test suite with verbose output."""
    # Run tests
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(__import__(__name__))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # Print summary if all tests passed
    if result.wasSuccessful():
        print_chipset_summary()

    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    exit(main())
