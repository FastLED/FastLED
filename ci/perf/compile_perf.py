#!/usr/bin/env python3
"""
Compile performance profiling for FastLED headers.

Usage:
    python ci/perf/compile_perf.py [--output=json|text] [--save-trace]

Options:
    --output=FORMAT     Output format (json or text, default: text)
    --save-trace        Save the clang trace JSON for debugging
    --compiler=PATH     Path to clang++ (default: clang++)
    --threshold=MS      Highlight headers slower than threshold (default: 50)
    --check-thresholds  Check thresholds and exit with error if exceeded
"""

import json
import os
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


class CompilePerfAnalyzer:
    def __init__(self, trace_json_path: str):
        """Parse clang -ftime-trace JSON output."""
        with open(trace_json_path, "r") as f:
            self.trace_data = json.load(f)
        self.events = self.trace_data.get("traceEvents", [])

        # Build event index for faster lookups
        self._build_event_index()

    def _build_event_index(self) -> None:
        """Build indices for different event types."""
        self.source_events: List[Dict[str, Any]] = []
        self.parse_events: List[Dict[str, Any]] = []
        self.instantiate_events: List[Dict[str, Any]] = []
        self.phase_events: Dict[str, Dict[str, Any]] = {}

        for event in self.events:
            name = event.get("name", "")

            if name == "Source":
                self.source_events.append(event)
            elif name.startswith("Parse"):
                self.parse_events.append(event)
            elif name.startswith("Instantiate"):
                self.instantiate_events.append(event)
            elif name in ["Frontend", "Backend", "ExecuteCompiler"]:
                self.phase_events[name] = event

    def get_total_time(self) -> float:
        """Get total compilation time in milliseconds."""
        exec_event = self.phase_events.get("ExecuteCompiler")
        if exec_event:
            return exec_event.get("dur", 0) / 1000.0
        return 0.0

    def get_phase_times(self) -> Dict[str, float]:
        """Get time spent in each compilation phase."""
        result: Dict[str, float] = {}

        # Main phases
        for phase in ["Frontend", "Backend"]:
            event = self.phase_events.get(phase)
            if event:
                result[phase] = event.get("dur", 0) / 1000.0

        # Additional breakdown
        source_time = sum(e.get("dur", 0) for e in self.source_events) / 1000.0
        result["Source"] = source_time

        template_time = sum(e.get("dur", 0) for e in self.instantiate_events) / 1000.0
        result["Templates"] = template_time

        # Try to extract lexing/preprocessing
        lex_events = [e for e in self.events if e.get("name", "").startswith("Lex")]
        result["Lexing"] = sum(e.get("dur", 0) for e in lex_events) / 1000.0

        preprocess_events = [
            e for e in self.events if "Preprocess" in e.get("name", "")
        ]
        result["Preprocessing"] = (
            sum(e.get("dur", 0) for e in preprocess_events) / 1000.0
        )

        return result

    def _normalize_path(self, path: str) -> str:
        """Normalize path for comparison (handle Windows/Unix paths)."""
        # Remove drive letters and normalize separators
        path = path.replace("\\", "/")
        if len(path) > 2 and path[1] == ":":
            path = path[2:]
        return path

    def _extract_header_name(self, path: str) -> str:
        """Extract just the header filename or relative path within project."""
        norm_path = self._normalize_path(path)

        # Try to extract relative to src/
        if "/src/" in norm_path:
            parts = norm_path.split("/src/")
            if len(parts) > 1:
                return parts[-1]

        # Otherwise just return the filename
        return Path(path).name

    def get_header_times(self, max_depth: int = 2) -> Dict[str, Any]:
        """
        Get header parse times with nesting information.

        Returns:
            Dict mapping header name to:
            {
                'time': float,        # milliseconds
                'path': str,          # full path
                'depth': int,         # nesting level
                'parent': str,        # parent header
                'children': List[str] # child headers
            }
        """
        headers: Dict[str, Dict[str, Any]] = {}

        # Sort events by timestamp
        sorted_sources = sorted(self.source_events, key=lambda e: e.get("ts", 0))

        # Build parent-child relationships
        stack: List[Tuple[str, int, int]] = []  # (path, start_ts, end_ts)

        for event in sorted_sources:
            path = event.get("args", {}).get("detail", "")
            if not path:
                continue

            ts = event.get("ts", 0)
            dur = event.get("dur", 0)
            end_ts = ts + dur

            # Find parent (last event that contains this timestamp)
            parent = None
            depth = 0

            # Clean up stack (remove events that have ended)
            while stack and stack[-1][2] <= ts:
                stack.pop()

            if stack:
                parent = stack[-1][0]
                depth = len(stack)

            stack.append((path, ts, end_ts))

            header_name = self._extract_header_name(path)

            if header_name not in headers:
                headers[header_name] = {
                    "time": dur / 1000.0,
                    "path": path,
                    "depth": depth,
                    "parent": self._extract_header_name(parent) if parent else None,
                    "children": [],
                }
            else:
                # Accumulate time if seen multiple times
                headers[header_name]["time"] += dur / 1000.0

            # Add to parent's children
            if parent:
                parent_name = self._extract_header_name(parent)
                if (
                    parent_name in headers
                    and header_name not in headers[parent_name]["children"]
                ):
                    headers[parent_name]["children"].append(header_name)

        return headers

    def get_fastled_headers(self) -> List[Tuple[str, float]]:
        """Get all FastLED-specific headers (src/fl/*, src/*.h)."""
        headers = self.get_header_times()
        fastled_headers: List[Tuple[str, float]] = []

        for name, info in headers.items():
            path = self._normalize_path(info["path"])
            # Check if it's a FastLED header
            if "/src/" in path or "fastled" in path.lower() or name.startswith("fl/"):
                fastled_headers.append((name, info["time"]))

        return sorted(fastled_headers, key=lambda x: x[1], reverse=True)

    def get_top_operations(self, n: int = 20) -> List[Tuple[str, str, float]]:
        """
        Get top N slowest operations.

        Returns:
            List of (operation_type, detail, time_ms)
            e.g., ('ParseClass', 'fl::string', 129.4)
        """
        operations: List[Tuple[str, str, float]] = []

        for event in self.events:
            name = event.get("name", "")
            dur = event.get("dur", 0)
            detail = event.get("args", {}).get("detail", "")

            # Skip very short operations
            if dur < 1000:  # Less than 1ms
                continue

            # Include interesting operations
            if any(
                keyword in name
                for keyword in ["Parse", "Instantiate", "Source", "Template"]
            ):
                if name == "Source":
                    detail = self._extract_header_name(detail)
                operations.append((name, detail, dur / 1000.0))

        # Sort by time and return top N
        operations.sort(key=lambda x: x[2], reverse=True)
        return operations[:n]

    def build_header_tree(self) -> Dict[str, Any]:
        """Build nested tree of header includes up to 2 levels deep."""
        headers = self.get_header_times(max_depth=2)

        # Find root headers (depth 0 or 1)
        tree: Dict[str, Any] = {}

        for name, info in headers.items():
            if info["depth"] <= 1:
                tree[name] = {"time": info["time"], "children": {}}

                # Add children
                for child_name in info["children"]:
                    if child_name in headers:
                        child_info = headers[child_name]
                        tree[name]["children"][child_name] = {
                            "time": child_info["time"],
                            "children": {},
                        }

                        # Add grandchildren
                        for grandchild_name in child_info["children"]:
                            if grandchild_name in headers:
                                grandchild_info = headers[grandchild_name]
                                tree[name]["children"][child_name]["children"][
                                    grandchild_name
                                ] = {"time": grandchild_info["time"]}

        return tree

    def generate_report(
        self, format_type: str = "text", threshold_ms: float = 50.0
    ) -> str:
        """Generate human-readable report."""
        if format_type == "json":
            return json.dumps(self.generate_json(), indent=2)

        # Text report
        lines: List[str] = []

        total_time = self.get_total_time()
        phases = self.get_phase_times()
        fastled_headers = self.get_fastled_headers()
        top_ops = self.get_top_operations(20)

        # Header
        lines.append("=" * 80)
        lines.append("FASTLED COMPILATION PERFORMANCE REPORT")
        lines.append("=" * 80)
        lines.append(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append(f"Compiler: {os.environ.get('COMPILER', 'clang++')}")
        lines.append(f"File: ci/perf/test_compile.cpp")
        lines.append("")

        # Summary
        lines.append("SUMMARY")
        lines.append("-" * 80)
        lines.append(f"Total Compilation Time:    {total_time:,.2f} ms")
        frontend_time = phases.get("Frontend", 0)
        backend_time = phases.get("Backend", 0)
        if total_time > 0:
            lines.append(
                f"Frontend Time:             {frontend_time:,.2f} ms ({frontend_time / total_time * 100:.1f}%)"
            )
            lines.append(
                f"Backend Time:              {backend_time:,.2f} ms ({backend_time / total_time * 100:.1f}%)"
            )
        lines.append("")

        # Compilation Phases
        lines.append("COMPILATION PHASES")
        lines.append("-" * 80)
        for phase_name in ["Source", "Templates", "Lexing", "Preprocessing"]:
            phase_time = phases.get(phase_name, 0)
            if phase_time > 0 and total_time > 0:
                lines.append(
                    f"{phase_name:25s} {phase_time:8.2f} ms ({phase_time / total_time * 100:5.1f}%)"
                )
        lines.append("")

        # FastLED Headers (Level 1)
        lines.append("FASTLED HEADERS (Level 1 - Direct Includes)")
        lines.append("-" * 80)
        for name, time_ms in fastled_headers[:20]:
            flag = " (SLOW)" if time_ms > threshold_ms else ""
            lines.append(f"  {time_ms:6.1f} ms - {name}{flag}")
        lines.append("")

        # Header Tree (Level 2)
        lines.append("FASTLED HEADERS (Level 2 - Nested Includes)")
        lines.append("-" * 80)
        tree = self.build_header_tree()

        # Show top level headers with their children
        sorted_tree = sorted(tree.items(), key=lambda x: x[1]["time"], reverse=True)
        for root_name, root_info in sorted_tree[:10]:
            if root_info["time"] > 5.0:  # Only show significant headers
                lines.append(f"{root_name} ({root_info['time']:.1f}ms)")

                children = sorted(
                    root_info["children"].items(),
                    key=lambda x: x[1]["time"],
                    reverse=True,
                )
                for i, (child_name, child_info) in enumerate(children[:5]):
                    is_last = i == len(children[:5]) - 1
                    prefix = "  └─" if is_last else "  ├─"
                    lines.append(
                        f"{prefix} {child_info['time']:6.1f} ms - {child_name}"
                    )

                if root_info["children"]:
                    lines.append("")

        # Top 20 Slowest Operations
        lines.append("TOP 20 SLOWEST OPERATIONS")
        lines.append("-" * 80)
        for i, (op_type, detail, time_ms) in enumerate(top_ops, 1):
            lines.append(f"{i:3d}. {time_ms:7.1f} ms - {op_type}: {detail}")
        lines.append("")

        # Performance Flags
        lines.append("PERFORMANCE FLAGS")
        lines.append("-" * 80)

        slow_headers = [
            (name, time) for name, time in fastled_headers if time > threshold_ms
        ]
        if slow_headers:
            lines.append(f"⚠️  SLOW HEADERS (>{threshold_ms}ms):")
            for name, time_ms in slow_headers[:10]:
                lines.append(f"    - {name} ({time_ms:.1f}ms)")
        else:
            lines.append(f"✓  No headers exceed {threshold_ms}ms threshold")

        lines.append("")

        template_time = phases.get("Templates", 0)
        if total_time > 0 and template_time / total_time < 0.15:
            lines.append("✓  Template instantiation time is acceptable (<15% of total)")
        elif total_time > 0:
            lines.append(
                f"⚠️  Template instantiation time is high ({template_time / total_time * 100:.1f}% of total)"
            )

        lex_time = phases.get("Lexing", 0)
        if total_time > 0 and lex_time / total_time < 0.10:
            lines.append("✓  Lexing time is acceptable (<10% of total)")

        lines.append("")

        # Recommendations
        lines.append("RECOMMENDATIONS")
        lines.append("-" * 80)
        if slow_headers:
            for i, (name, time_ms) in enumerate(slow_headers[:3], 1):
                lines.append(f"{i}. Consider optimizing {name} ({time_ms:.1f}ms)")
        else:
            lines.append("No major performance issues detected.")

        lines.append("")
        lines.append("=" * 80)

        return "\n".join(lines)

    def generate_json(self) -> Dict[str, Any]:
        """Generate JSON output for GitHub Actions artifacts."""
        total_time = self.get_total_time()
        phases = self.get_phase_times()
        fastled_headers = self.get_fastled_headers()
        top_ops = self.get_top_operations(20)

        warnings: List[str] = []

        # Check for slow headers
        for name, time_ms in fastled_headers:
            if time_ms > 150:
                warnings.append(f"Header {name} exceeds 150ms ({time_ms:.1f}ms)")
            elif time_ms > 50:
                warnings.append(f"Header {name} exceeds 50ms ({time_ms:.1f}ms)")

        # Check template instantiation
        template_time = phases.get("Templates", 0)
        if total_time > 0 and template_time / total_time > 0.15:
            warnings.append(
                f"Template instantiation time is high ({template_time / total_time * 100:.1f}%)"
            )

        return {
            "total_time": total_time,
            "phases": phases,
            "top_headers": [
                {"name": name, "time": time} for name, time in fastled_headers[:20]
            ],
            "top_operations": [
                {"type": op_type, "detail": detail, "time": time}
                for op_type, detail, time in top_ops
            ],
            "warnings": warnings,
            "timestamp": datetime.now().isoformat(),
        }


def compile_with_trace(
    source_file: Path, compiler: str = "clang++", include_dir: Optional[Path] = None
) -> Path:
    """
    Compile source file with -ftime-trace.

    Returns path to generated .json trace file.
    """
    output_file = source_file.with_suffix(".o")
    trace_file = source_file.with_suffix(".json")

    # Remove old files
    if output_file.exists():
        output_file.unlink()
    if trace_file.exists():
        trace_file.unlink()

    # Build compile command
    cmd = [
        compiler,
        "-std=c++14",  # C++14 required for MSVC headers
        "-c",
        str(source_file),
        "-o",
        str(output_file),
        "-ftime-trace",
    ]

    # Add include directory
    if include_dir:
        cmd.extend(["-I", str(include_dir)])
    else:
        # Default to src/ directory
        src_dir = source_file.parent.parent.parent / "src"
        if src_dir.exists():
            cmd.extend(["-I", str(src_dir)])

    # Add common defines for FastLED
    cmd.extend(
        [
            "-DFASTLED_INTERNAL",
            "-DFASTLED_TESTING",  # Use testing mode (simpler platform dependencies)
            "-Wno-everything",  # Suppress all warnings for cleaner output
        ]
    )

    print(f"Compiling with: {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

        if result.returncode != 0:
            print(f"Compilation failed:")
            print(result.stderr)
            sys.exit(1)

        if not trace_file.exists():
            print(f"Error: Trace file not generated at {trace_file}")
            print("Make sure your compiler supports -ftime-trace (Clang 9+)")
            sys.exit(1)

        return trace_file

    except subprocess.TimeoutExpired:
        print("Compilation timed out after 60 seconds")
        sys.exit(1)
    except FileNotFoundError:
        print(f"Error: Compiler '{compiler}' not found")
        print("Please specify the correct compiler with --compiler=PATH")
        sys.exit(1)


def check_thresholds(analyzer: CompilePerfAnalyzer, thresholds_file: Path) -> bool:
    """
    Check if compilation metrics exceed thresholds.

    Returns True if all checks pass, False otherwise.
    """
    if not thresholds_file.exists():
        print(f"Warning: Thresholds file not found at {thresholds_file}")
        return True

    with open(thresholds_file, "r") as f:
        thresholds = json.load(f)

    total_time = analyzer.get_total_time()
    phases = analyzer.get_phase_times()
    fastled_headers = analyzer.get_fastled_headers()

    errors: List[str] = []
    warnings: List[str] = []

    # Check total compile time
    max_total = thresholds.get("total_compile_time_ms", 2000)
    if total_time > max_total:
        errors.append(
            f"Total compile time {total_time:.1f}ms exceeds threshold {max_total}ms"
        )

    # Check header thresholds
    error_threshold = thresholds.get("header_error_threshold_ms", 150)
    warning_threshold = thresholds.get("header_warning_threshold_ms", 50)
    known_slow = thresholds.get("known_slow_headers", {})

    for name, time_ms in fastled_headers:
        # Skip known slow headers if within their specific threshold
        if name in known_slow:
            specific_threshold = known_slow[name]
            if time_ms > specific_threshold:
                warnings.append(
                    f"Known slow header {name} ({time_ms:.1f}ms) exceeds its threshold ({specific_threshold}ms)"
                )
            continue

        if time_ms > error_threshold:
            errors.append(
                f"Header {name} ({time_ms:.1f}ms) exceeds error threshold ({error_threshold}ms)"
            )
        elif time_ms > warning_threshold:
            warnings.append(
                f"Header {name} ({time_ms:.1f}ms) exceeds warning threshold ({warning_threshold}ms)"
            )

    # Check template instantiation percentage
    template_time = phases.get("Templates", 0)
    max_template_pct = thresholds.get("template_instantiation_percent", 15)
    if total_time > 0 and (template_time / total_time * 100) > max_template_pct:
        warnings.append(
            f"Template instantiation time ({template_time / total_time * 100:.1f}%) exceeds {max_template_pct}%"
        )

    # Print results
    if errors:
        print("\n❌ ERRORS:")
        for error in errors:
            print(f"  - {error}")

    if warnings:
        print("\n⚠️  WARNINGS:")
        for warning in warnings:
            print(f"  - {warning}")

    if not errors and not warnings:
        print("\n✓ All threshold checks passed")

    return len(errors) == 0


def main() -> None:
    """Main entry point."""
    # Parse arguments
    args = sys.argv[1:]
    output_format = "text"
    save_trace = False
    compiler = "clang++"
    threshold = 50.0
    check_thresholds_only = False

    for arg in args:
        if arg.startswith("--output="):
            output_format = arg.split("=", 1)[1]
        elif arg == "--save-trace":
            save_trace = True
        elif arg.startswith("--compiler="):
            compiler = arg.split("=", 1)[1]
        elif arg.startswith("--threshold="):
            threshold = float(arg.split("=", 1)[1])
        elif arg == "--check-thresholds":
            check_thresholds_only = True
        elif arg in ["--help", "-h"]:
            print(__doc__)
            sys.exit(0)

    # Find project root and source file
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    source_file = script_dir / "test_compile.cpp"
    thresholds_file = script_dir / "thresholds.json"

    if not source_file.exists():
        print(f"Error: Test file not found at {source_file}")
        sys.exit(1)

    # Store compiler in env for report
    os.environ["COMPILER"] = compiler

    # Compile with trace
    print("Compiling test file with -ftime-trace...")
    trace_file = compile_with_trace(
        source_file, compiler, include_dir=project_root / "src"
    )

    print(f"Trace file generated: {trace_file}")

    # Parse trace
    print("Analyzing trace data...")
    analyzer = CompilePerfAnalyzer(str(trace_file))

    # Check thresholds if requested
    if check_thresholds_only:
        success = check_thresholds(analyzer, thresholds_file)
        sys.exit(0 if success else 1)

    # Generate report
    report = analyzer.generate_report(output_format, threshold)
    print(report)

    # Clean up trace file unless save requested
    if not save_trace:
        trace_file.unlink()
        output_file = source_file.with_suffix(".o")
        if output_file.exists():
            output_file.unlink()


if __name__ == "__main__":
    main()
