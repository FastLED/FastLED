#!/usr/bin/env python3
"""
Compile performance profiling for FastLED headers.

This script analyzes compilation time using Clang's -ftime-trace feature and
reports two types of timing measurements:

TIMING MEASUREMENT TYPES:
-------------------------
1. CUMULATIVE TIME (Total Include Time):
   - Measures the TOTAL time from when the compiler starts processing a header
     until it finishes, INCLUDING all nested headers it includes
   - Example: FastLED.h cumulative time = time for FastLED.h itself + all its
     includes (controller.h, fl/stdint.h, etc.) + all their nested includes
   - This represents the REAL WORLD impact on user sketch compilation
   - Used for threshold checking (reflects actual user experience)

2. DIRECT TIME (Header-Only Time):
   - Measures ONLY the time spent processing the header's own content,
     EXCLUDING nested includes
   - Example: FastLED.h direct time = only time parsing FastLED.h code itself
   - Useful for identifying which specific header has complex code
   - Displayed in reports for detailed analysis

WHY WE CHECK CUMULATIVE TIME:
-----------------------------
When a user includes FastLED.h in their sketch, they experience the full
cumulative time including all nested dependencies. This is what matters for
developer experience, so thresholds are checked against cumulative times.

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
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Optional


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
        self.source_events: list[dict[str, Any]] = []
        self.parse_events: list[dict[str, Any]] = []
        self.instantiate_events: list[dict[str, Any]] = []
        self.phase_events: dict[str, dict[str, Any]] = {}

        # First pass: collect all events
        # Second pass: match begin/end pairs for async events
        begin_events: dict[int, dict[str, Any]] = {}

        for event in self.events:
            name = event.get("name", "")
            phase = event.get("ph", "")

            # Handle async begin/end pairs (Source events use this)
            if phase == "b":
                # Store begin event by its ID
                event_id = event.get("id", 0)
                begin_events[event_id] = event
                continue
            elif phase == "e":
                # Match with begin event and create complete event
                event_id = event.get("id", 0)
                if event_id in begin_events:
                    begin = begin_events[event_id]
                    # Create complete event with duration
                    complete = begin.copy()
                    complete["dur"] = event.get("ts", 0) - begin.get("ts", 0)
                    complete["ph"] = "X"  # Mark as complete
                    event = complete  # Use this for further processing
                    del begin_events[event_id]
                else:
                    continue

            # Now process the event (either originally complete or made complete)
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

    def get_phase_times(self) -> dict[str, float]:
        """Get time spent in each compilation phase."""
        result: dict[str, float] = {}

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

    def get_header_times(self, max_depth: int = 2) -> dict[str, Any]:
        """
        Get header parse times with nesting information.

        IMPORTANT: Returns CUMULATIVE time (total time including all nested includes).
        This is the full duration from when the compiler starts processing the header
        until it finishes, including all headers it includes recursively.

        Returns:
            Dict mapping header name to:
            {
                'time': float,        # CUMULATIVE milliseconds (includes nested headers)
                'path': str,          # full path
                'depth': int,         # nesting level
                'parent': str,        # parent header
                'children': List[str] # child headers
            }
        """
        headers: dict[str, dict[str, Any]] = {}

        # Sort events by timestamp
        sorted_sources = sorted(self.source_events, key=lambda e: e.get("ts", 0))

        # Build parent-child relationships
        stack: list[tuple[str, int, int]] = []  # (path, start_ts, end_ts)

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

    def get_fastled_headers(self) -> list[tuple[str, float]]:
        """
        Get all FastLED-specific headers with their CUMULATIVE compile times.

        Returns list of (header_name, cumulative_time_ms) tuples sorted by time.
        Cumulative time includes all nested headers, representing the real cost
        to users when they #include that header.
        """
        headers = self.get_header_times()
        fastled_headers: list[tuple[str, float]] = []

        for name, info in headers.items():
            path = self._normalize_path(info["path"])
            # Check if it's a FastLED header
            if "/src/" in path or "fastled" in path.lower() or name.startswith("fl/"):
                fastled_headers.append((name, info["time"]))

        return sorted(fastled_headers, key=lambda x: x[1], reverse=True)

    def get_top_operations(self, n: int = 20) -> list[tuple[str, str, float]]:
        """
        Get top N slowest operations.

        Returns:
            List of (operation_type, detail, time_ms)
            e.g., ('ParseClass', 'fl::string', 129.4)
        """
        operations: list[tuple[str, str, float]] = []

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

    def build_header_tree(self, max_depth: int = 3) -> dict[str, Any]:
        """Build nested tree of header includes up to max_depth levels deep."""
        headers = self.get_header_times(max_depth=max_depth + 1)

        def build_subtree(header_name: str, current_depth: int) -> dict[str, Any]:
            """Recursively build tree for a header and its children."""
            if header_name not in headers:
                return {"time": 0, "children": {}}

            info = headers[header_name]
            node: dict[str, Any] = {"time": info["time"], "children": {}}

            # Only recurse if we haven't hit max depth
            if current_depth < max_depth:
                for child_name in info["children"]:
                    node["children"][child_name] = build_subtree(
                        child_name, current_depth + 1
                    )

            return node

        # Find root headers (depth 0 or 1)
        tree: dict[str, Any] = {}
        for name, info in headers.items():
            if info["depth"] <= 1:
                tree[name] = build_subtree(name, 0)

        return tree

    def generate_report(
        self, format_type: str = "text", threshold_ms: float = 50.0
    ) -> str:
        """Generate human-readable report."""
        if format_type == "json":
            return json.dumps(self.generate_json(), indent=2)

        # Text report
        lines: list[str] = []

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
        lines.append("File: ci/perf/test_compile.cpp")
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

        # Header Tree (Multiple levels)
        lines.append("FASTLED HEADERS (Nested Includes - Up to 10 Levels)")
        lines.append("-" * 80)
        tree = self.build_header_tree(max_depth=10)

        def format_tree(
            node_dict: dict[str, Any],
            prefix: str = "",
            is_last: bool = True,
            depth: int = 0,
            max_children: int = 5,
        ) -> list[str]:
            """Recursively format tree structure."""
            result: list[str] = []

            # Sort children by time
            children = sorted(
                node_dict.items(),
                key=lambda x: x[1]["time"],
                reverse=True,
            )

            # Limit children shown at each level
            children = children[:max_children]

            for i, (name, info) in enumerate(children):
                is_last_child = i == len(children) - 1

                # Format current node
                if depth == 0:
                    # Root level - no prefix
                    result.append(f"{name} ({info['time']:.1f}ms)")
                else:
                    # Child levels - use tree formatting
                    connector = "└─" if is_last_child else "├─"
                    result.append(
                        f"{prefix}{connector} {info['time']:6.1f} ms - {name}"
                    )

                # Recurse for children
                if info["children"]:
                    # Determine prefix for next level
                    if depth == 0:
                        next_prefix = "  "
                    else:
                        extension = "  " if is_last_child else "│ "
                        next_prefix = prefix + extension

                    result.extend(
                        format_tree(
                            info["children"],
                            prefix=next_prefix,
                            is_last=is_last_child,
                            depth=depth + 1,
                            max_children=max_children,
                        )
                    )

            return result

        # Show top level headers with their nested children
        sorted_tree = sorted(tree.items(), key=lambda x: x[1]["time"], reverse=True)
        top_headers = {k: v for k, v in sorted_tree[:10] if v["time"] > 5.0}

        for line in format_tree(top_headers, depth=0):
            lines.append(line)

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

    def generate_json(self) -> dict[str, Any]:
        """Generate JSON output for GitHub Actions artifacts."""
        total_time = self.get_total_time()
        phases = self.get_phase_times()
        fastled_headers = self.get_fastled_headers()
        top_ops = self.get_top_operations(20)

        warnings: list[str] = []

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
            "-DFASTLED_STUB_IMPL",  # Use stub platform (all pins valid)
            "-Wno-everything",  # Suppress all warnings for cleaner output
        ]
    )

    print(f"Compiling with: {' '.join(cmd)}", file=sys.stderr)

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

        if result.returncode != 0:
            print("Compilation failed:", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            sys.exit(1)

        if not trace_file.exists():
            print(f"Error: Trace file not generated at {trace_file}", file=sys.stderr)
            print(
                "Make sure your compiler supports -ftime-trace (Clang 9+)",
                file=sys.stderr,
            )
            sys.exit(1)

        return trace_file

    except subprocess.TimeoutExpired:
        print("Compilation timed out after 60 seconds", file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print(f"Error: Compiler '{compiler}' not found", file=sys.stderr)
        print(
            "Please specify the correct compiler with --compiler=PATH", file=sys.stderr
        )
        sys.exit(1)


def check_thresholds(analyzer: CompilePerfAnalyzer, thresholds_file: Path) -> bool:
    """
    Check if compilation metrics exceed thresholds.

    This function checks ONLY the aggregate/total compilation time against the
    configured threshold. Individual header times are not checked as they can
    vary significantly between compilations due to caching and other factors.
    The aggregate time is what matters for real-world user experience.

    Returns True if all checks pass, False otherwise.
    """
    if not thresholds_file.exists():
        print(
            f"Warning: Thresholds file not found at {thresholds_file}", file=sys.stderr
        )
        return True

    with open(thresholds_file, "r") as f:
        thresholds = json.load(f)

    total_time = analyzer.get_total_time()

    errors: list[str] = []

    # Check total compile time (this is the only check that matters)
    max_total = thresholds.get("total_compile_time_ms", 2000)
    if total_time > max_total:
        errors.append(
            f"Total compile time {total_time:.1f}ms exceeds threshold {max_total}ms"
        )

    # Print results
    if errors:
        print("\n❌ ERRORS:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
    else:
        print(
            f"\n✓ All threshold checks passed - Total compile time: {total_time:.1f}ms (threshold: {max_total}ms)",
            file=sys.stderr,
        )

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
    print("Compiling test file with -ftime-trace...", file=sys.stderr)
    trace_file = compile_with_trace(
        source_file, compiler, include_dir=project_root / "src"
    )

    print(f"Trace file generated: {trace_file}", file=sys.stderr)

    # Parse trace
    print("Analyzing trace data...", file=sys.stderr)
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
