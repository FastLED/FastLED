"""Extract the JChalka verifier CSV into a clean unit-test fixture.

Reads the medium-patch-set verifier output (preserved on issue #3422) and
emits `tests/misc/golden/verifier_known_bad.csv`, a stable vendored data file
the colorimetric strategy harness reads at host-test time.

Source: https://docs.google.com/spreadsheets/d/1GYxE3jcknTTUx9mYUp4wGJd8SEkBKI-sdCajryqsx6U

Output schema (columns):
    patch                  - patch name, e.g. "h180_s100_v015"
    r_in16, g_in16, b_in16 - source linear RGB input (u16 / 65535-scale)
    exp_x, exp_y           - target CIE 1931 xy chromaticity
    cf_r16, cf_g16, cf_b16, cf_w16
                           - legacy analytical_strict_rgbw16 snapshot from
                             the archived verifier sheet, not the current
                             canonical FastLED closed-form reference
    meas_x, meas_y, meas_Y - colorimeter measurement of the resulting LED
    dE                     - measured dE for that legacy analytical output
    status                 - "ok" if dE within tolerance, "fail" if known-bad,
                             "skip" for degenerate (black, etc.)

Provenance is preserved verbatim in `tests/misc/golden/README.md`.
"""

import argparse
import csv
from pathlib import Path


def unpack_rgbw(s: str) -> tuple[int, int, int, int]:  # noqa: DCT002
    """Parse 'r/g/b/w' slash-separated u16 quad into (r, g, b, w)."""
    parts = s.split("/")
    if len(parts) != 4:
        return (0, 0, 0, 0)
    return (int(parts[0]), int(parts[1]), int(parts[2]), int(parts[3]))


def safe_int(s: str) -> int:
    try:
        return int(s)
    except (ValueError, TypeError):
        return 0


def safe_float(s: str) -> float:
    if s is None or s == "":
        return 0.0
    try:
        return float(s)
    except ValueError:
        return 0.0


def status_from_ok(ok_marker: str) -> str:
    if ok_marker == "✓":
        return "ok"
    if ok_marker == "✗":
        return "fail"
    return "skip"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--source",
        default=r"C:\Users\niteris\dev\fastled9\.claude\tmp\jchalka_medium_verifier.csv",
        help="Input verifier CSV (medium patch set)",
    )
    ap.add_argument(
        "--out",
        default=r"C:\Users\niteris\dev\fastled9\tests\misc\golden\verifier_known_bad.csv",
        help="Output fixture CSV",
    )
    args = ap.parse_args()

    src = Path(args.source)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    with src.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    fieldnames = [
        "patch",
        "r_in16",
        "g_in16",
        "b_in16",
        "exp_x",
        "exp_y",
        "cf_r16",
        "cf_g16",
        "cf_b16",
        "cf_w16",
        "meas_x",
        "meas_y",
        "meas_Y",
        "dE",
        "status",
    ]

    counts = {"ok": 0, "fail": 0, "skip": 0}
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for r in rows:
            cf_rgbw = unpack_rgbw(r["analytical_strict_rgbw16"])
            status = status_from_ok(r.get("ok", ""))
            counts[status] += 1
            w.writerow(
                {
                    "patch": r["patch"],
                    "r_in16": safe_int(r["r16"]),
                    "g_in16": safe_int(r["g16"]),
                    "b_in16": safe_int(r["b16"]),
                    "exp_x": f"{safe_float(r['exp_x']):.6f}",
                    "exp_y": f"{safe_float(r['exp_y']):.6f}",
                    "cf_r16": cf_rgbw[0],
                    "cf_g16": cf_rgbw[1],
                    "cf_b16": cf_rgbw[2],
                    "cf_w16": cf_rgbw[3],
                    "meas_x": f"{safe_float(r['meas_x']):.6f}",
                    "meas_y": f"{safe_float(r['meas_y']):.6f}",
                    "meas_Y": f"{safe_float(r['meas_Y']):.6f}",
                    "dE": f"{safe_float(r['dE']):.4f}" if r.get("dE") else "",
                    "status": status,
                }
            )

    print(f"wrote {out}")
    print(f"  total rows: {sum(counts.values())}")
    for k, v in counts.items():
        print(f"  {k:>5}: {v}")


if __name__ == "__main__":
    main()
