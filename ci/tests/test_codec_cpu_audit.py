from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "ci" / "codec_cpu" / "audit.py"
SPEC = importlib.util.spec_from_file_location("codec_cpu_audit", MODULE_PATH)
assert SPEC and SPEC.loader
AUDIT = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = AUDIT
SPEC.loader.exec_module(AUDIT)


def test_checked_in_cpu_trend_is_complete() -> None:
    trend = AUDIT.load_trend()
    AUDIT.validate_trend(trend)


def test_five_percent_regression_gate() -> None:
    baseline = {"helix": 1_000_000, "minimp3-float": 2_000_000}
    AUDIT.check_regression(baseline, dict(baseline))
    changed = dict(baseline)
    changed["minimp3-float"] = 2_100_001
    with pytest.raises(RuntimeError, match="regressed"):
        AUDIT.check_regression(baseline, changed)


def test_llvm_function_stage_classification() -> None:
    assert AUDIT.classify_stage("_ZL10L3_huffman") == "huffman"
    assert AUDIT.classify_stage("_Z7FDCT32Pii") == "synthesis"
    assert AUDIT.classify_stage("_ZL11L3_imdct_gr") == "imdct"
    assert AUDIT.classify_stage("unrelated") is None


def test_float_mac_counts_one_accumulate_for_two_products() -> None:
    llvm = """
define void @L3_antialias() {
entry:
  %left = fmul float %a, %b
  %right = fmul float %c, %d
  %sum = fadd float %left, %right
  ret void
}
"""
    result = AUDIT.instrument_llvm_ir(llvm, "minimp3-float")
    assert result.text.count("i32 5, i32 1, i32 0") == 2
    assert result.text.count("i32 5, i32 0, i32 1") == 1


def test_fused_minimp3_huffman_products_are_dequantized_operations() -> None:
    llvm = """
define void @L3_huffman() {
entry:
  %scaled = fmul float %sample, %scale
  ret void
}
"""
    result = AUDIT.instrument_llvm_ir(llvm, "minimp3-float")
    assert "i32 2, i32 1, i32 0" in result.text
    assert result.stage_sites["huffman"] == 1


def test_helix_mulshift_accumulation_counts_one_mac() -> None:
    llvm = """
define i32 @MULSHIFT32(i32 %x, i32 %y) {
entry:
  %wide_x = sext i32 %x to i64
  %wide_y = sext i32 %y to i64
  %product = mul i64 %wide_x, %wide_y
  %scaled = trunc i64 %product to i32
  ret i32 %scaled
}
define i32 @AntiAlias(i32 %a, i32 %b, i32 %c) {
entry:
  %left = call i32 @MULSHIFT32(i32 %a, i32 %b)
  %right = call i32 @MULSHIFT32(i32 %a, i32 %c)
  %sum = add i32 %left, %right
  ret i32 %sum
}
"""
    result = AUDIT.instrument_llvm_ir(llvm, "helix")
    assert result.text.count("i32 -1, i32 1, i32 0") == 1
    assert result.text.count("i32 5, i32 0, i32 1") == 1


def test_helix_mulshift_accumulation_survives_o0_spills_and_casts() -> None:
    llvm = """
define i64 @AntiAlias(i32 %a, i32 %b, ptr %slot) {
entry:
  %product = call i32 @MULSHIFT32(i32 %a, i32 %b)
  store i32 %product, ptr %slot
  %loaded = load i32, ptr %slot
  %wide = sext i32 %loaded to i64
  %first = add i64 %wide, 1
  %second = sub i64 %first, 2
  ret i64 %second
}
"""
    result = AUDIT.instrument_llvm_ir(llvm, "helix")
    assert result.text.count("i32 5, i32 0, i32 1") == 1


def test_host_counters_combine_cycle_median_and_callgrind() -> None:
    cycles = {"helix": 100.0, "minimp3-float": 200.0}
    callgrind = {
        backend: {"instructions": 250, "branch_misses": 4} for backend in AUDIT.BACKENDS
    }
    counters = AUDIT.compose_host_counters(cycles, callgrind)
    assert counters["helix"] == {
        "cycles": 100.0,
        "instructions": 250,
        "branch_misses": 4,
        "ipc": 2.5,
    }
    assert counters["minimp3-float"]["ipc"] == 1.25


def test_host_affinity_is_fail_closed() -> None:
    assert AUDIT.make_affinity_prefix("/usr/bin/taskset", 3) == [
        "/usr/bin/taskset",
        "-c",
        "3",
    ]
    with pytest.raises(RuntimeError, match="requires taskset"):
        AUDIT.make_affinity_prefix(None, 3)


def test_linux_compiler_discovery_rejects_windows_cache() -> None:
    assert AUDIT.cached_compiler_candidates("posix") == []
    assert all(
        candidate.suffix == ".exe"
        for candidate in AUDIT.cached_compiler_candidates("nt")
    )


def test_instruction_parser_counts_inner_backedge() -> None:
    disassembly = """
00000000 <kernel>:
   0:  00 00       nop
   2:  00 00       nop
   4:  fc ff       bne 2 <kernel+0x2>
   6:  00 00       ret
"""
    metrics = AUDIT.parse_disassembly(disassembly, "kernel")
    assert metrics == {"instructions": 4, "inner_loop_instructions": 2}


def test_instruction_parser_excludes_arm_calls() -> None:
    disassembly = """
00000000 <kernel>:
   0:  b500       push {lr}
   2:  f7ff fffe  bl 0 <callee>
   6:  bd00       pop {pc}
"""
    metrics = AUDIT.parse_disassembly(disassembly, "kernel")
    assert metrics == {"instructions": 3, "inner_loop_instructions": 0}


def test_instruction_parser_counts_xtensa_zero_overhead_loop() -> None:
    disassembly = """
00000000 <kernel>:
   0:  108276     loop a2, 9 <kernel+0x9>
   3:  223a       add.n a2, a2, a3
   5:  334a       add.n a3, a3, a4
   7:  f01d       retw.n
   9:  f01d       retw.n
"""
    metrics = AUDIT.parse_disassembly(disassembly, "kernel")
    assert metrics == {"instructions": 5, "inner_loop_instructions": 3}


def test_operation_report_requires_every_stage() -> None:
    counts = {stage: {"multiplies": 1, "macs": 0} for stage in AUDIT.STAGES}
    AUDIT.validate_operation_counts(counts)
    del counts["synthesis"]
    with pytest.raises(RuntimeError, match="missing operation stage"):
        AUDIT.validate_operation_counts(counts)


def test_operation_ledger_is_exact_per_frame() -> None:
    operations = {
        "frames": 4,
        "stages": {
            stage: {
                "total_multiplies": 10,
                "total_macs": 4,
                "multiplies_per_frame": 2.5,
                "macs_per_frame": 1.0,
            }
            for stage in AUDIT.STAGES
        },
    }
    AUDIT.validate_operations("helix", operations)
    operations["stages"]["imdct"]["multiplies_per_frame"] = 2.4
    with pytest.raises(RuntimeError, match="inexact derived operation metric"):
        AUDIT.validate_operations("helix", operations)


def test_callgrind_attribution_keeps_codec_functions() -> None:
    annotation = """
100 (50.0%) /repo/src/third_party/minimp3/minimp3.h:mp3d_synth
100 (50.0%) src/third_party/minimp3/minimp3.h:mp3d_synth [/tmp/a.out]
80 (40.0%) /lib/libc.so:memcpy
"""
    assert AUDIT.parse_callgrind_attribution(annotation) == {
        "src/third_party/minimp3/minimp3.h:mp3d_synth": 100
    }


def test_callgrind_parser_rejects_mismatched_event_values(tmp_path: Path) -> None:
    output = tmp_path / "callgrind.out"
    output.write_text("events: Ir Bc Bcm\nsummary: 100 20\n", encoding="utf-8")
    with pytest.raises(ValueError, match="shorter"):
        AUDIT.parse_callgrind(output)


def test_callgrind_parser_prefers_complete_totals(tmp_path: Path) -> None:
    output = tmp_path / "callgrind.out"
    output.write_text(
        "events: Ir Bc Bcm\nsummary: 0 0 0\ntotals: 100 20 3\n",
        encoding="utf-8",
    )
    assert AUDIT.parse_callgrind(output).summary == {"Ir": 100, "Bc": 20, "Bcm": 3}


def test_trend_gate_rejects_more_than_five_percent() -> None:
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    counters = current["backends"]["helix"]["host"]["counter_median"]
    counters["cycles"] *= 1.051
    counters["ipc"] = round(counters["instructions"] / counters["cycles"], 6)
    with pytest.raises(RuntimeError, match="helix/counter/cycles regressed"):
        AUDIT.check_trend(baseline, current)


def test_trend_validation_rejects_inconsistent_derived_counters() -> None:
    trend = AUDIT.load_trend()
    trend["backends"]["helix"]["host"]["counter_median"]["ipc"] = 0.5
    with pytest.raises(RuntimeError, match="inexact derived host IPC"):
        AUDIT.validate_trend(trend)


def test_trend_gate_fails_closed_on_unknown_host() -> None:
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    current["environment"]["cpu_model"] = "different hosted runner"
    current["host_key"] = AUDIT.environment_key(current["environment"])
    with pytest.raises(RuntimeError, match="host baseline is unknown"):
        AUDIT.check_trend(baseline, current)


def test_trend_gate_selects_known_host_baseline() -> None:
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    current["environment"]["cpu_model"] = "alternate hosted runner"
    current["host_key"] = AUDIT.environment_key(current["environment"])
    primary_counters = baseline["backends"]["helix"]["host"]["counter_median"]
    primary_counters["cycles"] /= 2
    primary_counters["ipc"] = round(
        primary_counters["instructions"] / primary_counters["cycles"], 6
    )
    baseline["host_baselines"] = {
        current["host_key"]: {
            "environment": current["environment"],
            "hosts": json.loads(
                json.dumps(
                    {
                        backend: current["backends"][backend]["host"]
                        for backend in AUDIT.BACKENDS
                    }
                )
            ),
        }
    }
    AUDIT.check_trend(baseline, current)
    counters = current["backends"]["helix"]["host"]["counter_median"]
    counters["cycles"] *= 1.051
    counters["ipc"] = round(counters["instructions"] / counters["cycles"], 6)
    with pytest.raises(RuntimeError, match="helix/counter/cycles regressed"):
        AUDIT.check_trend(baseline, current)


def test_trend_gate_checks_callgrind_function_share() -> None:
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    functions = current["backends"]["helix"]["host"]["callgrind"]["functions"]
    function = next(iter(functions))
    functions[function] = functions[function] * 1051 // 1000
    with pytest.raises(RuntimeError, match=r"callgrind-function/.+ regressed"):
        AUDIT.check_trend(baseline, current)


def test_parse_args_accepts_explicit_argv() -> None:
    args = AUDIT.parse_args(["--operations"])
    assert args.operations
