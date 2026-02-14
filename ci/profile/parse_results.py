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

    def load_results(self) -> None:
        """Load and parse JSON results"""
        with open(self.results_file) as f:
            data = json.load(f)

        for entry in data:
            self.results.append(ProfileResult(**entry))

    def compute_statistics(self) -> dict[str, float]:
        """Compute best/median/worst/stdev"""
        if not self.results:
            return {
                "best": 0,
                "median": 0,
                "worst": 0,
                "stdev": 0,
                "count": 0,
            }

        ns_per_call = [r.ns_per_call for r in self.results]

        return {
            "best": min(ns_per_call),
            "median": statistics.median(ns_per_call),
            "worst": max(ns_per_call),
            "stdev": statistics.stdev(ns_per_call) if len(ns_per_call) > 1 else 0,
            "count": len(ns_per_call),
        }

    def generate_report(self) -> str:
        """Generate markdown report for AI consumption"""
        stats = self.compute_statistics()
        target = self.results[0].target if self.results else "unknown"

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

    def export_for_ai(self) -> dict[str, Any]:
        """Export structured data for AI consumption"""
        stats = self.compute_statistics()
        return {
            "target": self.results[0].target if self.results else "unknown",
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
