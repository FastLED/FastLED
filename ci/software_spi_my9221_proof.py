"""Closed MY9221 host schedule contract; never grants full configuration proof.

Pure preparation/diagnostic helpers return no authenticated evidence. Actual
execution is exclusively the fixed managed runner in software_spi_init_proof.
"""

import re
from pathlib import Path


CONTROLLER = "src/fl/chipsets/my9221.h"
SERIALIZER = "src/platforms/shared/my9221_typed_emitter.h"
TEST = "tests/fl/channels/spi_legacy_golden.cpp"
CASE = "MY9221 public controller observes DDR and four timed DI latch pulses"
CHAIN_CASE = "MY9221 partial BGR chains repeat with final HIGH and cascade stop"
CASES = (
    CASE,
    CHAIN_CASE,
    "MY9221 real dynamic reserve failure emits no partial frame and retries",
    "MY9221 public emission guards edges and restores prior interrupt state",
)
_MUTATIONS = {
    "missing-latch-high-wait": (CONTROLLER, b"Timing::template waitNanoseconds<80>();"),
    "missing-latch-low-wait": (CONTROLLER, b"Timing::template waitNanoseconds<240>();"),
    "missing-data-setup-wait": (SERIALIZER, b"Timing::template waitNanoseconds<10>();"),
    "missing-data-hold-phase-wait": (
        SERIALIZER,
        b"Timing::template waitNanoseconds<50>();",
    ),
    "missing-cascade-stop-wait": (
        CONTROLLER,
        b"Timing::waitMicroseconds(mStopDelayUs);",
    ),
}
STAGES = ("baseline", *_MUTATIONS, "restored")


def prepare_mutants(original: dict[str, bytes]) -> dict[str, dict[str, bytes]]:
    if not isinstance(original, dict) or set(original) != {CONTROLLER, SERIALIZER}:
        raise ValueError(
            "MY9221 mutation source set must match fixed production headers"
        )
    if any(not isinstance(value, bytes) for value in original.values()):
        raise ValueError("MY9221 mutation sources must be actual bytes")
    mutants = {}
    for stage, (path, call) in _MUTATIONS.items():
        if original[path].count(call) != 1:
            raise ValueError(
                "MY9221 mutation requires one exact production call pattern"
            )
        payload = dict(original)
        payload[path] = payload[path].replace(call, b"", 1)
        mutants[stage] = payload
    return mutants


def validate_stage_sequence(names: tuple[str, ...]) -> None:
    if type(names) is not tuple or names != STAGES:
        raise ValueError(
            "MY9221 stage sequence must contain baseline, five mutants, restored"
        )


def cases_for_stage(stage: str) -> tuple[str, ...]:
    if stage not in STAGES:
        raise ValueError("unknown MY9221 stage")
    if stage in {"baseline", "restored"}:
        return CASES
    return (CHAIN_CASE if stage == "missing-cascade-stop-wait" else CASE,)


def _line(oracle: bytes, expression: str) -> int:
    lines = oracle.decode("utf-8").splitlines()
    matches = [
        index + 1 for index, text in enumerate(lines) if text.strip() == expression
    ]
    if len(matches) != 1:
        raise ValueError("missing or ambiguous bound MY9221 oracle assertion")
    return matches[0]


def _failure_families(stage: str, oracle: bytes) -> dict[int, str]:
    expressions = {
        "missing-latch-high-wait": [
            ("FL_REQUIRE_GT(latch_high_ns, static_cast<fl::u64>(70));", "1 > 70")
        ],
        "missing-latch-low-wait": [
            ("FL_CHECK_GT(low_end - low.ns, static_cast<fl::u64>(230));", "1 > 230")
        ],
        "missing-data-setup-wait": [
            (
                "FL_CHECK_GE(edges[i].ns - previous_data_change, static_cast<fl::u64>(10));",
                "1 >= 10",
            )
        ],
        "missing-data-hold-phase-wait": [
            ("FL_CHECK_GE(phase, static_cast<fl::u64>(50));", "12 >= 50"),
            (
                "FL_CHECK_GE(next_data_change - edges[i].ns, static_cast<fl::u64>(10));",
                "1 >= 10",
            ),
        ],
        "missing-cascade-stop-wait": [
            ("FL_CHECK_GT(state.ns - pulses.back().ns, stop_bound_ns);", "240 > 250")
        ],
    }
    return {
        _line(oracle, expression): diagnostic
        for expression, diagnostic in expressions[stage]
    }


def validate_output(
    stage: str, case: str, returncode: int, output: str, *, oracle: bytes | None = None
) -> None:
    """Reject malformed/incidental diagnostics; this alone authenticates nothing."""
    if case not in cases_for_stage(stage):
        raise ValueError("MY9221 stage used wrong designated case")
    mutation = stage not in {"baseline", "restored"}
    if type(returncode) is not int or returncode != (1 if mutation else 0):
        raise ValueError("MY9221 runtime exit does not match fixed stage")
    if re.search(
        r"\bfatal\b|\berror:|sanitizer|runtime error:|terminate called|"
        r"segmentation fault|traceback|uncaught exception|unhandled exception|"
        r"ninja: build stopped|\bTIMEOUT\b|\bCACHED\b",
        output,
        re.IGNORECASE,
    ):
        raise ValueError("MY9221 incidental build/runtime error or cached diagnostic")
    lines = output.splitlines()
    summaries = [line for line in lines if "[doctest] test cases:" in line]
    passes = [line for line in lines if line.startswith("PASS:")]
    fails = [line for line in lines if line.startswith("FAIL:")]
    assertions = [
        line for line in lines if re.match(r"\s+.*\.(?:cpp|hpp|h):\d+: ", line)
    ]
    requires = [line for line in lines if "FAILED:" in line]
    if not mutation:
        if (
            passes != [f"PASS: {case}"]
            or fails
            or assertions
            or requires
            or len(summaries) != 1
            or not re.fullmatch(
                r"\[doctest\] test cases: +1 \| +1 passed \| +0 failed", summaries[0]
            )
        ):
            raise ValueError(
                "MY9221 baseline/restored lacks exact single-case runtime success"
            )
        return
    if oracle is None:
        oracle = (Path(__file__).resolve().parents[1] / TEST).read_bytes()
    allowed = _failure_families(stage, oracle)
    # Fatal REQUIRE exits immediately: selected-case and binary identity are
    # separately bound by the actual runner, no invented unreachable summary.
    if stage == "missing-latch-high-wait":
        line, diagnostic = next(iter(allowed.items()))
        expected = (
            rf"REQUIRE FAILED: .*spi_legacy_golden\.cpp:{line}: {re.escape(diagnostic)}"
        )
        if (
            len(requires) != 1
            or not re.fullmatch(expected, requires[0])
            or summaries
            or passes
            or fails
            or assertions
        ):
            raise ValueError("MY9221 lacks sole intended latch-high runtime assertion")
        return
    declaration = _line(oracle, f'FL_TEST_CASE("{case}") {{')
    if (
        passes
        or requires
        or len(fails) != 1
        or not re.fullmatch(
            rf"FAIL: {re.escape(case)} at .*spi_legacy_golden\.cpp:{declaration}",
            fails[0],
        )
        or len(summaries) != 1
        or not re.fullmatch(
            r"\[doctest\] test cases: +1 \| +0 passed \| +1 failed", summaries[0]
        )
    ):
        raise ValueError("MY9221 mutation lacks exact failed designated case/summary")
    if not assertions:
        raise ValueError("MY9221 mutation lacks intended oracle failure")
    observed = []
    for index, text in enumerate(assertions):
        match = re.fullmatch(r"  .*spi_legacy_golden\.cpp:(\d+): (.+)", text)
        # The actual selected setup mutant first parks both pins before its
        # first clock edge: four model GPIO operations, then one per change.
        # Only this first assertion at the bound setup oracle may report4ns.
        setup_preamble = (
            stage == "missing-data-setup-wait"
            and index == 0
            and match is not None
            and allowed.get(int(match[1])) == "1 >= 10"
            and match[2] == "4 >= 10"
        )
        if not match or (allowed.get(int(match[1])) != match[2] and not setup_preamble):
            raise ValueError("MY9221 extra or incorrect assertion/failure family/bound")
        observed.append(match[2])
    required = (
        "12 >= 50"
        if stage == "missing-data-hold-phase-wait"
        else next(iter(allowed.values()))
    )
    if required not in observed:
        raise ValueError("MY9221 missing intended runtime assertion/bound")
