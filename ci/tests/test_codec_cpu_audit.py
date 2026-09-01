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
    baseline = {"minimp3-float": 2_000_000, "minimp3-fixed": 2_500_000}
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


def test_integer_product_selection_ignores_address_and_size_math() -> None:
    """`mul` is ambiguous in a way `fmul` is not: at -O0 the fixed-point
    decoder's arithmetic and the compiler's address/size arithmetic share the
    opcode. Only the sign-extended 64-bit product that is *not* consumed by a
    getelementptr or a call is a sample multiply."""
    body = [
        "  %a64 = sext i32 %a to i64",
        "  %b64 = sext i32 %b to i64",
        "  %prod = mul nsw i64 %a64, %b64",          # Q-format: counted
        "  %coef = sext i32 %c to i64",
        "  %scaled = mul nsw i64 %coef, 37489",      # literal tap: counted
        "  %idx = sext i32 %i to i64",
        "  %off = mul nsw i64 %idx, 4",              # address math: rejected
        "  %p = getelementptr inbounds i32, ptr %base, i64 %off",
        "  %n = sext i32 %count to i64",
        "  %bytes = mul nsw i64 %n, 72",             # memcpy size: rejected
        "  %r = call ptr @memcpy(ptr %d, ptr %s, i64 %bytes)",
        "  %wide = mul nsw i64 %x, %y",              # neither sign-extended
    ]
    products = AUDIT._integer_product_lines(body)

    assert products.values == {"%prod", "%scaled"}
    assert products.lines == {2, 4}


def test_integer_mac_counts_one_accumulate_for_two_products() -> None:
    """Mirrors the float MAC test. Two products feeding one add is two
    multiplies and one multiply-accumulate, not three multiplies."""
    llvm = """
define void @L3_antialias() {
entry:
  %a64 = sext i32 %a to i64
  %b64 = sext i32 %b to i64
  %left = mul nsw i64 %a64, %b64
  %c64 = sext i32 %c to i64
  %right = mul nsw i64 %c64, %b64
  %sum = add nsw i64 %left, %right
  ret void
}
"""
    result = AUDIT.instrument_llvm_ir(llvm, "minimp3-fixed")
    assert result.text.count("i32 5, i32 1, i32 0") == 2
    assert result.text.count("i32 5, i32 0, i32 1") == 1


def test_float_instrumentation_does_not_fire_on_the_fixed_backend() -> None:
    """The two backends must not cross-instrument: a float build has no
    integer sample multiplies and a fixed build has no fmul."""
    float_ir = """
define void @L3_antialias() {
entry:
  %p = fmul float %a, %b
  ret void
}
"""
    # Counting the call form, not the `declare` line, which also names it.
    assert AUDIT.instrument_llvm_ir(float_ir, "minimp3-float").text.count(
        "call void @fastled_mp3_cpu_operation"
    ) == 1
    with pytest.raises(RuntimeError, match="no arithmetic sites"):
        AUDIT.instrument_llvm_ir(float_ir, "minimp3-fixed")


def test_host_counters_combine_cycle_median_and_callgrind() -> None:
    cycles = {"minimp3-float": 200.0, "minimp3-fixed": 250.0}
    callgrind = {
        backend: {"instructions": 250, "branch_misses": 4} for backend in AUDIT.BACKENDS
    }
    counters = AUDIT.compose_host_counters(cycles, callgrind)
    assert counters["minimp3-float"] == {
        "cycles": 200.0,
        "instructions": 250,
        "branch_misses": 4,
        "ipc": 1.25,
    }


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
    AUDIT.validate_operations("minimp3-float", operations)
    operations["stages"]["imdct"]["multiplies_per_frame"] = 2.4
    with pytest.raises(RuntimeError, match="inexact derived operation metric"):
        AUDIT.validate_operations("minimp3-float", operations)


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
    """Retired instruction count is a property of the code, so it is gated."""
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    counters = current["backends"]["minimp3-float"]["host"]["counter_median"]
    host = current["backends"]["minimp3-float"]["host"]
    counters["instructions"] = int(counters["instructions"] * 1.051)
    # perf and Callgrind instruction counts are cross-checked against each
    # other, so both have to move or validation rejects the fixture first.
    host["callgrind"]["instructions"] = int(host["callgrind"]["instructions"] * 1.051)
    counters["ipc"] = round(counters["instructions"] / counters["cycles"], 6)
    with pytest.raises(
        RuntimeError, match="minimp3-float/counter/instructions regressed"
    ):
        AUDIT.check_trend(baseline, current)


def test_cycles_are_reported_but_do_not_fail_the_build(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Cycles, the IPC derived from them, and per-stage wall-clock timings are
    properties of the machine, not the code (FastLED#4130). Note that
    `counter_median` instructions and branch misses are *not* in this set:
    validate_trend requires them to equal the Callgrind figures, so they are
    simulated and stay gated. Cycles are the only genuinely hardware-measured
    quantity. The runner pool
    moves them by more than the 5% budget between machines reporting the same
    CPU model, which failed PRs that provably could not have caused it. They
    are recorded loudly and no longer gated."""
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    counters = current["backends"]["minimp3-float"]["host"]["counter_median"]
    counters["cycles"] = int(counters["cycles"] * 1.30)
    counters["ipc"] = round(counters["instructions"] / counters["cycles"], 6)
    for stage in current["backends"]["minimp3-float"]["host"]["stage_ns_median"]:
        current["backends"]["minimp3-float"]["host"]["stage_ns_median"][stage] *= 1.30

    AUDIT.check_trend(baseline, current)  # a 30% swing must not raise

    printed = capsys.readouterr().out
    assert "UNGATED:minimp3-float/counter/cycles" in printed
    assert "drift=+30.00%" in printed
    assert "worth a look" in printed


def test_trend_validation_rejects_inconsistent_derived_counters() -> None:
    trend = AUDIT.load_trend()
    trend["backends"]["minimp3-float"]["host"]["counter_median"]["ipc"] = 0.5
    with pytest.raises(RuntimeError, match="inexact derived host IPC"):
        AUDIT.validate_trend(trend)


def test_unknown_host_still_gates_the_deterministic_metrics(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """An unknown runner no longer fails closed (FastLED#4130).

    It used to, and that was right while cycles were gated: a cycle count from
    an unrecognised machine compares to nothing. After the demotion every gated
    figure is simulated -- the operation ledger, the cross-compiled codegen
    bounds, and the Callgrind numbers that counter_median is required to equal
    -- so the primary baseline applies to any host. The ubuntu-24.04 pool has
    presented at least five CPU models, and each new one used to block
    unrelated PRs until someone harvested a baseline.

    What must not happen is an unknown host silently gating nothing."""
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    current["environment"]["cpu_model"] = "some runner nobody has measured"
    current["host_key"] = AUDIT.environment_key(current["environment"])

    AUDIT.check_trend(baseline, current)
    assert "UNKNOWN-HOST:" in capsys.readouterr().out

    # ...and the deterministic gate still fires on that same unknown host.
    host = current["backends"]["minimp3-float"]["host"]
    counters = host["counter_median"]
    counters["instructions"] = int(counters["instructions"] * 1.051)
    host["callgrind"]["instructions"] = int(host["callgrind"]["instructions"] * 1.051)
    counters["ipc"] = round(counters["instructions"] / counters["cycles"], 6)
    with pytest.raises(
        RuntimeError, match="minimp3-float/counter/instructions regressed"
    ):
        AUDIT.check_trend(baseline, current)


def test_trend_gate_selects_known_host_baseline() -> None:
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    current["environment"]["cpu_model"] = "alternate hosted runner"
    current["host_key"] = AUDIT.environment_key(current["environment"])
    primary_counters = baseline["backends"]["minimp3-float"]["host"]["counter_median"]
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
    counters = current["backends"]["minimp3-float"]["host"]["counter_median"]
    host = current["backends"]["minimp3-float"]["host"]
    counters["instructions"] = int(counters["instructions"] * 1.051)
    # perf and Callgrind instruction counts are cross-checked against each
    # other, so both have to move or validation rejects the fixture first.
    host["callgrind"]["instructions"] = int(host["callgrind"]["instructions"] * 1.051)
    counters["ipc"] = round(counters["instructions"] / counters["cycles"], 6)
    with pytest.raises(
        RuntimeError, match="minimp3-float/counter/instructions regressed"
    ):
        AUDIT.check_trend(baseline, current)


def test_trend_gate_checks_callgrind_function_share() -> None:
    baseline = AUDIT.load_trend()
    current = json.loads(json.dumps(baseline))
    functions = current["backends"]["minimp3-float"]["host"]["callgrind"]["functions"]
    function = next(iter(functions))
    functions[function] = functions[function] * 1051 // 1000
    with pytest.raises(RuntimeError, match=r"callgrind-function/.+ regressed"):
        AUDIT.check_trend(baseline, current)


def test_parse_args_accepts_explicit_argv() -> None:
    args = AUDIT.parse_args(["--operations"])
    assert args.operations
