"""Tests for the USB selective-suspend preflight warning (FastLED #3864).

The check exists because a Windows-suspended port makes a board come back as
`Unknown USB Device (Device Descriptor Request Failed)` (code 43), whose stale
COM record then shows `health=phantom` — indistinguishable at a glance from a
wedged board, and the reason a real investigation went down the wrong path.
"""

from __future__ import annotations

from ci.autoresearch.usb_power import (
    absent_port_error,
    hub_chain_power_off,
    humanise_age,
    parse_presence,
    plan_selective_suspend_enabled,
    selective_suspend_warnings,
    warn_selective_suspend,
)


POWERCFG_ENABLED = """
Power Scheme GUID: 381b4222-f694-41f0-9685-ff5bb260df2e  (Balanced)
    Power Setting GUID: 48e6b7a6-50f5-4782-a5d4-53bb8f07e226  (USB selective suspend setting)
      Current AC Power Setting Index: 0x00000001
      Current DC Power Setting Index: 0x00000001
"""

POWERCFG_DISABLED = POWERCFG_ENABLED.replace("0x00000001", "0x00000000")


def _runner(mapping: dict[str, str]):
    """Fake command runner: first matching key in argv wins."""

    def run(cmd: list[str]) -> str:
        joined = " ".join(cmd)
        for needle, output in mapping.items():
            if needle in joined:
                return output
        return ""

    return run


def test_plan_setting_parsed_enabled_and_disabled() -> None:
    assert plan_selective_suspend_enabled(run=_runner({"powercfg": POWERCFG_ENABLED}))
    assert not plan_selective_suspend_enabled(
        run=_runner({"powercfg": POWERCFG_DISABLED})
    )


def test_plan_setting_is_none_when_undeterminable() -> None:
    """Unparseable output must be 'no opinion', never a false 'disabled'."""
    assert plan_selective_suspend_enabled(run=_runner({})) is None
    assert plan_selective_suspend_enabled(run=_runner({"powercfg": "garbage"})) is None


def test_hub_chain_parsed() -> None:
    ps = "USB\\VID_05E3&PID_0610\\7&3afc677d&0&1|True\nUSB\\ROOT_HUB30\\5&4087d53&0&0|False\n"
    chain = hub_chain_power_off("COM17", run=_runner({"powershell": ps}))
    assert chain == [
        ("USB\\VID_05E3&PID_0610\\7&3afc677d&0&1", True),
        ("USB\\ROOT_HUB30\\5&4087d53&0&0", False),
    ]


def test_hub_chain_empty_when_port_absent() -> None:
    assert hub_chain_power_off("COM17", run=_runner({"powershell": ""})) == []


def test_warns_on_specific_hub_when_resolvable() -> None:
    ps = "USB\\VID_05E3&PID_0610\\7&3afc677d&0&1|True\n"
    warnings = selective_suspend_warnings(
        "COM17", platform="win32", run=_runner({"powershell": ps})
    )
    assert any("hub chain for COM17" in w for w in warnings)
    assert any("05E3" in w for w in warnings)
    # remediation must always accompany the finding
    assert any("powercfg -setacvalueindex" in w for w in warnings)


def test_falls_back_to_plan_setting_when_hub_unresolvable() -> None:
    """The board is usually absent when someone is debugging this, so the
    per-device chain is empty; the plan-wide setting still tells us something."""
    warnings = selective_suspend_warnings(
        "COM17",
        platform="win32",
        run=_runner({"powershell": "", "powercfg": POWERCFG_ENABLED}),
    )
    assert any("active Windows power plan" in w for w in warnings)


def test_silent_when_nothing_is_suspendable() -> None:
    ps = "USB\\ROOT_HUB30\\5&4087d53&0&0|False\n"
    assert (
        selective_suspend_warnings(
            "COM17",
            platform="win32",
            run=_runner({"powershell": ps, "powercfg": POWERCFG_DISABLED}),
        )
        == []
    )


def test_no_op_off_windows() -> None:
    """Must stay silent on Linux/macOS CI rather than shelling out."""
    calls: list[list[str]] = []

    def run(cmd: list[str]) -> str:
        calls.append(cmd)
        return POWERCFG_ENABLED

    assert selective_suspend_warnings("COM17", platform="linux", run=run) == []
    assert calls == [], "must not shell out on non-Windows"


def test_default_runner_captures_output(monkeypatch) -> None:
    """Regression: RunningProcess.run defaults to capture_output=False, which
    streams to the console and leaves stdout empty — silently disabling every
    check here. The injected-runner tests cannot catch this."""
    import running_process

    from ci.autoresearch import usb_power

    seen: dict[str, object] = {}

    class _Result:
        stdout = "ok"

    def fake_run(args, **kwargs):
        seen.update(kwargs)
        return _Result()

    monkeypatch.setattr(running_process.RunningProcess, "run", staticmethod(fake_run))
    assert usb_power._run(["powercfg"]) == "ok"
    assert seen.get("capture_output") is True


def test_warn_never_raises(capsys) -> None:
    """A preflight hint must never be able to fail a HIL run."""

    def exploding(cmd: list[str]) -> str:
        raise RuntimeError("wmi exploded")

    warn_selective_suspend("COM17", run=exploding)  # must not propagate
    warn_selective_suspend(None, run=exploding)


# --- Absent-port preflight (the ~7-minute build that could only fail) --------


def test_parse_presence_reads_both_fields() -> None:
    assert parse_presence("True|42") == (True, 42)
    assert parse_presence("False|432000") == (False, 432000)


def test_parse_presence_unknown_is_not_absent() -> None:
    """No output, or an unparseable field, must never mean "not attached".

    A query failure that reported absence would abort a run against a board
    that is sitting there perfectly healthy.
    """
    assert parse_presence("") == (None, None)
    assert parse_presence("garbage with no pipe") == (None, None)
    assert parse_presence("Maybe|42") == (None, 42)
    # Present but no arrival date recorded — still a definite presence answer.
    assert parse_presence("True|") == (True, None)


def test_humanise_age_scales() -> None:
    assert humanise_age(45) == "45s"
    assert humanise_age(700) == "11m"
    assert humanise_age(7200) == "2h"
    assert humanise_age(432000) == "5d"


def test_absent_port_error_fires_only_on_definite_absence() -> None:
    win = {"platform": "win32"}
    assert absent_port_error("COM17", run=lambda cmd: "False|432000", **win) is not None
    assert absent_port_error("COM9", run=lambda cmd: "True|60", **win) is None
    # Unknown must be permissive, not fatal.
    assert absent_port_error("COM9", run=lambda cmd: "", **win) is None


def test_absent_port_error_no_op_off_windows() -> None:
    """Linux/macOS CI has no Get-PnpDevice; must never block a run there."""
    calls: list[list[str]] = []

    def run(cmd: list[str]) -> str:
        calls.append(cmd)
        return "False|432000"

    assert absent_port_error("COM17", platform="linux", run=run) is None
    assert calls == [], "must not shell out on non-Windows"


def test_absent_port_error_explains_stale_record_and_bootsel() -> None:
    """The message has to pre-empt the wrong conclusion, not just say "no"."""
    msg = absent_port_error("COM17", platform="win32", run=lambda cmd: "False|432000")
    assert msg is not None
    assert "5d ago" in msg
    assert "stale devnode, not a fault" in msg
    assert "BOOTSEL" in msg
