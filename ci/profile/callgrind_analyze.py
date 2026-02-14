#!/usr/bin/env python3
"""
Run profiler under Valgrind callgrind and analyze hotspots.

Usage:
    python ci/profile/callgrind_analyze.py profile_sincos16
"""

import subprocess
import sys
from pathlib import Path
from typing import Any


class CallgrindAnalyzer:
    def __init__(self, profiler_binary: Path):
        self.profiler_binary = profiler_binary
        self.callgrind_out = Path("callgrind.out.profile")

    def run_callgrind(self) -> bool:
        """Run profiler under callgrind"""
        cmd = [
            "valgrind",
            "--tool=callgrind",
            f"--callgrind-out-file={self.callgrind_out}",
            str(self.profiler_binary),
            "baseline",
        ]

        try:
            subprocess.run(cmd, check=True)
            return True
        except subprocess.CalledProcessError as e:
            print(f"Error running callgrind: {e}")
            return False
        except FileNotFoundError:
            print("Error: valgrind not found. Please install valgrind.")
            print("  On Ubuntu/Debian: sudo apt-get install valgrind")
            print("  On macOS: brew install valgrind")
            return False

    def analyze_hotspots(self) -> list[dict[str, Any]]:
        """Parse callgrind output and identify hot functions"""
        cmd = ["callgrind_annotate", str(self.callgrind_out)]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error running callgrind_annotate: {e}")
            return []
        except FileNotFoundError:
            print("Error: callgrind_annotate not found (part of valgrind package)")
            return []

        # Parse output to find functions consuming >5% of time
        hotspots: list[dict[str, Any]] = []
        for line in result.stdout.split("\n"):
            if "%" in line and "PROGRAM TOTALS" not in line:
                parts = line.split()
                if len(parts) >= 2:
                    try:
                        pct = float(parts[0].replace(",", ""))
                        if pct > 5.0:
                            func = " ".join(parts[1:])
                            hotspots.append({"function": func, "percentage": pct})
                    except ValueError:
                        pass

        return sorted(hotspots, key=lambda x: x["percentage"], reverse=True)

    def generate_report(self, hotspots: list[dict[str, Any]]) -> str:
        """Generate markdown report of hotspots"""
        report = "# Callgrind Hotspot Analysis\n\n"

        if not hotspots:
            report += "No hotspots found (functions consuming >5% of time)\n"
            return report

        report += "| Function | % of Time |\n"
        report += "|----------|----------|\n"

        for h in hotspots[:10]:  # Top 10
            report += f"| {h['function']} | {h['percentage']:.1f}% |\n"

        report += "\n## Recommendations\n\n"
        if hotspots and hotspots[0]["percentage"] > 30:
            report += f"- **Focus on {hotspots[0]['function']}** (consumes {hotspots[0]['percentage']:.1f}% of time)\n"

        return report


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: callgrind_analyze.py <profiler_binary>")
        return 1

    profiler = Path(sys.argv[1])
    if not profiler.exists():
        print(f"Error: {profiler} not found")
        return 1

    analyzer = CallgrindAnalyzer(profiler)

    print("Running callgrind (this may take a few minutes)...")
    if not analyzer.run_callgrind():
        return 1

    print("Analyzing hotspots...")
    hotspots = analyzer.analyze_hotspots()

    print(analyzer.generate_report(hotspots))
    return 0


if __name__ == "__main__":
    sys.exit(main())
