"""On-device FastLED.show() latency gate for AutoResearch (FastLED#5356).

#2994 shipped across many releases because nothing on the bench measured how
long ``FastLED.show()`` blocks. This gate runs the #2994 reproducer
(``timingDriftTest``: 35 WS2812 on the legacy ``addLeds`` path) after a
passing ``--legacy`` AutoResearch run and fails the run when ``show()``
regresses. A ``--legacy`` build binds the legacy templates to the one
selected driver, so the budget measures that driver; other modes skip.

Budgets live in ``tests/data/show_latency_budget.txt`` as
``<environment> <DRIVER> <max_mean_us>`` lines. A board/driver pair without a
budget skips the gate, and so does a malformed budget file (with a warning;
``ci/tests/test_show_latency_gate.py`` guards the checked-in file). Raising a
budget needs a written reason in that file, like the bloat baselines.

Two checks:
  * mean ``show()`` must not exceed the budget;
  * mean ``show()`` must stay under half the WS2812 wire time. A ``show()``
    that tracks wire time blocked on the transfer, the #2994 failure mode.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any

from colorama import Fore, Style


if TYPE_CHECKING:
    from ci.autoresearch.context import QuietContext, RunContext
    from ci.rpc_client import RpcClient


BUDGET_FILE = (
    Path(__file__).resolve().parents[2] / "tests" / "data" / "show_latency_budget.txt"
)

# timingDriftTest geometry: the #2994 reporter's 35-LED ring.
NUM_LEDS = 35
WIRE_US = NUM_LEDS * 24 * 1.25
# One fade sequence times ~255 show() calls (one per 5 ms step) and blocks
# ~2.3 s on the device (fade plus the sketch's trailing delay(1000)).
ITERATIONS = 2
RPC_TIMEOUT_S = 30.0
DEFAULT_PIN = 4


@dataclass(frozen=True)
class LatencyVerdict:
    ok: bool
    message: str


def parse_budgets(text: str) -> dict[tuple[str, str], int]:
    """Parse ``<environment> <DRIVER> <max_mean_us>`` lines; ``#`` comments."""
    budgets: dict[tuple[str, str], int] = {}
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 3 or not parts[2].isdigit():
            raise ValueError(
                f"show latency budget line {lineno}: expected "
                f"'<environment> <DRIVER> <max_mean_us>', got {raw!r}"
            )
        budgets[(parts[0], parts[1].upper())] = int(parts[2])
    return budgets


def load_budgets(path: Path = BUDGET_FILE) -> dict[tuple[str, str], int]:
    return parse_budgets(path.read_text(encoding="utf-8"))


def _counter_summary(result: dict[str, Any], mean_us: float) -> str:
    """Instructions per show() from the RISC-V perf counter (#5376).

    Code layout moves wall time by 10-20 us between builds; the instruction
    count does not, so per-change savings are judged on it. Cycles come from
    wall time x CPU MHz, so IPC shows how much of show() is stalls.
    """
    counted = int(result.get("show_instr_count") or 0)
    if counted <= 0:
        return ""
    instr = int(result.get("show_instr_total") or 0) / counted
    text = f"; {instr:.0f} instr/show"
    mhz = int(result.get("cpu_mhz") or 0)
    if mhz > 0 and mean_us > 0:
        cycles = mean_us * mhz
        text += f", {cycles:.0f} cycles/show (IPC {instr / cycles:.2f})"
    return text


def evaluate(result: dict[str, Any], budget_us: int) -> LatencyVerdict:
    """Judge one timingDriftTest result against a mean show() budget."""
    if not result.get("success", False):
        return LatencyVerdict(
            False,
            f"timingDriftTest failed: {result.get('error', 'unknown error')}"
            f" - {result.get('message', '')}",
        )
    count = int(result.get("show_count") or 0)
    total = result.get("show_total_us")
    if count <= 0 or total is None:
        return LatencyVerdict(False, "firmware reported no show() timing")
    mean = float(total) / count
    summary = (
        f"show() mean {mean:.1f} us over {count} calls "
        f"(min {result.get('show_min_us')}, max {result.get('show_max_us')}); "
        f"budget {budget_us} us"
    )
    summary += _counter_summary(result, mean)
    if mean >= WIRE_US / 2:
        return LatencyVerdict(
            False,
            f"{summary}; show() tracks the {WIRE_US:.0f} us wire time, "
            "so it blocks on the transfer (#2994)",
        )
    if mean > budget_us:
        return LatencyVerdict(False, f"{summary}; over budget")
    return LatencyVerdict(True, summary)


def gate_key(
    environment: str | None, drivers: list[str], legacy: bool
) -> tuple[str, str] | None:
    """Budget key for a run, or None when the gate does not apply."""
    if not legacy or environment is None or len(drivers) != 1:
        return None
    return (environment, drivers[0].upper())


def budget_for(key: tuple[str, str] | None) -> int | None:
    if key is None:
        return None
    try:
        budgets = load_budgets()
    except (OSError, ValueError) as exc:
        print(f"{Fore.YELLOW}⚠  show() latency gate skipped: {exc}{Style.RESET_ALL}")
        return None
    return budgets.get(key)


def resolve_gate(ctx: "RunContext") -> tuple[tuple[str, str], int] | None:
    """The (key, budget) the gate applies to this run, or None to skip."""
    key = gate_key(ctx.final_environment, ctx.drivers, bool(ctx.args.legacy))
    budget = budget_for(key)
    if key is None or budget is None:
        return None
    return key, budget


async def run_show_latency_gate(
    ctx: "RunContext",
    client: "RpcClient",
    qctx: "QuietContext",
    gate: tuple[tuple[str, str], int],
) -> bool:
    """Run the gate for a resolved (key, budget). True = pass."""
    key, budget = gate
    pin = ctx.effective_tx_pin if ctx.effective_tx_pin is not None else DEFAULT_PIN

    print(f"\n⏱  show() latency gate: {key[0]} {key[1]} (budget {budget} us)")
    response = await client.send(
        "timingDriftTest",
        args={"pin": pin, "numLeds": NUM_LEDS, "iterations": ITERATIONS},
        timeout=RPC_TIMEOUT_S,
    )
    data = response.data if isinstance(response.data, dict) else {}
    verdict = evaluate(data, budget)
    if verdict.ok:
        print(f"{Fore.GREEN}✓ {verdict.message}{Style.RESET_ALL}")
        qctx.emit(f"LATENCY PASS {verdict.message}")
    else:
        print(f"{Fore.RED}❌ show() latency gate: {verdict.message}{Style.RESET_ALL}")
        qctx.emit(f"FAILURE class=latency {verdict.message}")
    return verdict.ok
