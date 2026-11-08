"""Host-side checks for AutoResearch power-limit captures (FastLED #5139).

`runSingleTest` with `maxPowerMw` reports, for every captured frame, the
brightness the limiter reported (`expectedScale`), the modeled demand at full
brightness and the undimmable baseline (MCU + dark current), and the summed
source and wire bytes. The policy checks below judge the brightness measured
on the wire, independently of the firmware's limiter code:

* a budget that covers the full demand leaves the frame untouched (255);
* a budget over the baseline but under the demand dims the frame and its
  modeled draw stays within the budget (the budget is an upper bound);
* a budget at or under the baseline blanks the frame (nothing fits);
* the wire agrees with the brightness the limiter reported.
"""

from typing import Any


# Wire bytes are scale8 of the source; summed over a frame they reproduce the
# scalar to within rounding of the darkest channels.
_MEASURED_SCALE_TOLERANCE = 3


def measured_scale(frame: dict[str, Any]) -> int | None:
    """Brightness implied by the wire: 255 * sum(wire) / sum(source)."""
    raw = int(frame.get("sumRaw", 0))
    if raw <= 0:
        return None
    return round(255 * int(frame.get("sumWire", 0)) / raw)


def check_power_frames(result: dict[str, Any], sent_budget: object) -> list[str]:
    """Return policy violations for one runSingleTest response; [] if none.

    `sent_budget` is the request's maxPowerMw. When one was sent the response
    must carry the frames for it: firmware that predates the option ignores
    the key and passes unscaled, which would otherwise read as a PASS.
    """
    if not isinstance(sent_budget, int) or sent_budget <= 0:
        return []
    frames = result.get("powerFrames")
    budget = result.get("maxPowerMw")
    if budget != sent_budget or not isinstance(frames, list) or not frames:
        return [
            f"requested maxPowerMw={sent_budget} but the response has no power "
            "frames for it; reflash with firmware that supports maxPowerMw"
        ]
    errors: list[str] = []
    for index, frame in enumerate(frames):
        if not isinstance(frame, dict):
            errors.append(f"frame {index}: malformed power frame")
            continue
        reported = int(frame.get("expectedScale", -1))
        # Judge what reached the LEDs. The Channel API's frame plan is not the
        # scalar show() applies (it reports flux 0 under the baseline while the
        # wire carries the classic ratio), so the wire is the ground truth.
        wire = measured_scale(frame)
        scale = wire if wire is not None else reported
        full = int(frame.get("fullDemandMw", 0))
        baseline = int(frame.get("baselineMw", 0))
        lit_source = int(frame.get("sumRaw", 0)) > 0
        label = f"frame {index} (budget {budget} mW, full {full} mW, baseline {baseline} mW)"

        if budget >= full:
            if scale != 255:
                errors.append(f"{label}: budget covers demand but scale is {scale}")
        elif budget > baseline:
            if not 0 <= scale < 255:
                errors.append(
                    f"{label}: over-budget frame was not dimmed (scale {scale})"
                )
            # The budget is an upper bound: the scaled draw must fit, give or
            # take the wire measurement's rounding (one code of the demand).
            if (
                baseline + (full - baseline) * scale // 255
                > budget + (full - baseline) // 255 + 1
            ):
                errors.append(f"{label}: scale {scale} models above the budget")
        elif scale > 1:
            # Nothing fits a budget the baseline already spends.
            errors.append(f"{label}: budget under the baseline but scale {scale}")

        if wire is not None and abs(wire - reported) > _MEASURED_SCALE_TOLERANCE:
            errors.append(
                f"{label}: wire measures scale {wire}, limiter said {reported}"
            )
        if lit_source and scale > 0 and int(frame.get("sumWire", 0)) == 0:
            errors.append(f"{label}: wire was dark")
    return errors


def display_power_frames(result: dict[str, Any]) -> None:
    """Print one line per captured frame of a power-limit run."""
    frames = result.get("powerFrames")
    if not isinstance(frames, list):
        return
    print(f"   Power budget: {result.get('maxPowerMw')} mW")
    for index, frame in enumerate(frames):
        if not isinstance(frame, dict):
            continue
        wire = measured_scale(frame)
        print(
            f"     frame {index}: full {frame.get('fullDemandMw')} mW, "
            f"baseline {frame.get('baselineMw')} mW, "
            f"limiter scale {frame.get('expectedScale')}, "
            f"wire scale {wire if wire is not None else '-'}, "
            f"{'PASS' if frame.get('passed') else 'FAIL'}"
        )
