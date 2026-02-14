#!/usr/bin/env python3
"""
Parse profiler results and generate statistical analysis.

Usage:
    python ci/profile/parse_results.py profile_sincos16_results.json
"""

import json
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class ProfileResult:
    variant: str
    target: str
    total_calls: int
    total_time_ns: int
    ns_per_call: float
    calls_per_sec: float


class ResultAnalyzer:
    def __init__(self, results_file: Path):
        self.results_file = results_file
        self.results: list[ProfileResult] = []
        self.variants: dict[str, list[ProfileResult]] = {}

    def load_results(self) -> None:
        """Load and parse JSON results"""
        with open(self.results_file) as f:
            data = json.load(f)

        for entry in data:
            result = ProfileResult(**entry)
            self.results.append(result)
            # Group by variant for comparison tests
            if result.variant not in self.variants:
                self.variants[result.variant] = []
            self.variants[result.variant].append(result)

    def compute_statistics(
        self, results: list[ProfileResult] | None = None
    ) -> dict[str, float]:
        """Compute best/median/worst/stdev for a set of results"""
        if results is None:
            results = self.results

        if not results:
            return {
                "best": 0,
                "median": 0,
                "worst": 0,
                "stdev": 0,
                "count": 0,
            }

        ns_per_call = [r.ns_per_call for r in results]

        return {
            "best": min(ns_per_call),
            "median": statistics.median(ns_per_call),
            "worst": max(ns_per_call),
            "stdev": statistics.stdev(ns_per_call) if len(ns_per_call) > 1 else 0,
            "count": len(ns_per_call),
        }

    def generate_report(self) -> str:
        """Generate markdown report for AI consumption"""
        target = self.results[0].target if self.results else "unknown"

        # Check if this is a comparison test (multiple variants)
        if len(self.variants) > 1:
            return self._generate_comparison_report(target)
        else:
            return self._generate_single_variant_report(target)

    def _generate_single_variant_report(self, target: str) -> str:
        """Generate report for single-variant tests"""
        stats = self.compute_statistics()

        report = f"""
# Performance Profile: {target}

## Statistics ({stats["count"]} iterations)

| Metric | Value |
|--------|-------|
| **Best** | {stats["best"]:.2f} ns/call |
| **Median** | {stats["median"]:.2f} ns/call |
| **Worst** | {stats["worst"]:.2f} ns/call |
| **StdDev** | {stats["stdev"]:.2f} ns |
| **Calls/sec** | {1e9 / stats["median"]:.0f} |

## Interpretation

- **Best case**: {stats["best"]:.2f} ns/call (ideal conditions)
- **Typical**: {stats["median"]:.2f} ns/call (50th percentile)
- **Variance**: {stats["stdev"]:.2f} ns (lower is better)

## Recommendations

"""

        # Add recommendations based on performance
        median_ns = stats["median"]
        if median_ns > 1000:
            report += "- **High latency** (>1μs): Consider algorithmic improvements\n"
        elif median_ns > 100:
            report += (
                "- **Moderate latency** (100-1000ns): Look for micro-optimizations\n"
            )
        else:
            report += (
                "- **Low latency** (<100ns): Already well-optimized, focus elsewhere\n"
            )

        if stats["stdev"] / stats["median"] > 0.1:
            report += (
                "- **High variance**: Results inconsistent, may need more iterations\n"
            )

        return report

    def _generate_comparison_report(self, target: str) -> str:
        """Generate report for comparison tests with multiple variants"""
        report = f"""
# Performance Comparison: {target}

"""

        # Compute statistics for each variant
        variant_stats: dict[str, dict[str, float]] = {}
        for variant_name, variant_results in self.variants.items():
            variant_stats[variant_name] = self.compute_statistics(variant_results)

        # Display table for each variant
        report += "## Variant Performance\n\n"
        for variant_name in sorted(self.variants.keys()):
            stats = variant_stats[variant_name]
            report += f"### {variant_name} ({stats['count']} iterations)\n\n"
            report += "| Metric | Value |\n"
            report += "|--------|-------|\n"
            report += f"| **Best** | {stats['best']:.2f} ns/call |\n"
            report += f"| **Median** | {stats['median']:.2f} ns/call |\n"
            report += f"| **Worst** | {stats['worst']:.2f} ns/call |\n"
            report += f"| **StdDev** | {stats['stdev']:.2f} ns |\n"
            report += f"| **Calls/sec** | {1e9 / stats['median']:.0f} |\n\n"

        # Comparison analysis
        report += "## Comparison\n\n"

        # Find fastest and slowest
        sorted_variants = sorted(variant_stats.items(), key=lambda x: x[1]["median"])
        fastest = sorted_variants[0]
        slowest = sorted_variants[-1]

        report += f"**Fastest**: {fastest[0]} ({fastest[1]['median']:.2f} ns/call)\n\n"
        report += f"**Slowest**: {slowest[0]} ({slowest[1]['median']:.2f} ns/call)\n\n"

        if len(sorted_variants) >= 2:
            speedup = slowest[1]["median"] / fastest[1]["median"]
            report += (
                f"**Speedup**: {speedup:.2f}x faster ({fastest[0]} vs {slowest[0]})\n\n"
            )

        # Relative performance table
        report += "| Variant | Median (ns) | Relative | Speedup |\n"
        report += "|---------|-------------|----------|----------|\n"
        baseline_median = fastest[1]["median"]
        for variant_name, stats in sorted_variants:
            relative = stats["median"] / baseline_median
            speedup = baseline_median / stats["median"]
            if relative == 1.0:
                report += f"| **{variant_name}** | {stats['median']:.2f} | 1.00x (baseline) | - |\n"
            else:
                report += f"| {variant_name} | {stats['median']:.2f} | {relative:.2f}x | {speedup:.2f}x |\n"

        return report

    def export_for_ai(self) -> dict[str, Any]:
        """Export structured data for AI consumption"""
        target = self.results[0].target if self.results else "unknown"

        # Handle comparison tests
        if len(self.variants) > 1:
            variant_stats: dict[str, Any] = {}
            for variant_name, variant_results in self.variants.items():
                variant_stats[variant_name] = self.compute_statistics(variant_results)

            # Compute speedup
            sorted_variants = sorted(
                variant_stats.items(), key=lambda x: x[1]["median"]
            )
            fastest = sorted_variants[0]
            slowest = sorted_variants[-1]
            speedup = slowest[1]["median"] / fastest[1]["median"]

            return {
                "target": target,
                "comparison": True,
                "variants": variant_stats,
                "fastest": fastest[0],
                "slowest": slowest[0],
                "speedup": round(speedup, 2),
            }
        else:
            # Single variant
            stats = self.compute_statistics()
            return {
                "target": target,
                "comparison": False,
                "statistics": stats,
                "raw_results": [
                    {"ns_per_call": r.ns_per_call, "calls_per_sec": r.calls_per_sec}
                    for r in self.results
                ],
            }


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: parse_results.py <results.json>")
        return 1

    results_file = Path(sys.argv[1])
    if not results_file.exists():
        print(f"Error: {results_file} not found")
        return 1

    analyzer = ResultAnalyzer(results_file)
    analyzer.load_results()

    # Print markdown report
    print(analyzer.generate_report())

    # Export AI-readable JSON
    ai_data = analyzer.export_for_ai()
    ai_file = results_file.with_suffix(".ai.json")
    with open(ai_file, "w") as f:
        json.dump(ai_data, f, indent=2)

    print(f"\nAI-readable data exported to: {ai_file}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
