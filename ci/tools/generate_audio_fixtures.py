#!/usr/bin/env python3
"""Generate OGG Vorbis audio fixtures for VocalDetector integration tests.

Downloads CC0/public domain stems from archive.org, processes them with ffmpeg,
and creates mix variants at controlled vocal levels.

Usage:
    uv run python ci/tools/generate_audio_fixtures.py [--output tests/data/audio]
    uv run python ci/tools/generate_audio_fixtures.py --synth-only  # Skip downloads

Output:
    tests/data/audio/stems/   - Individual instrument stems (10s each)
    tests/data/audio/mixes/   - Mixed variants at different vocal levels
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SAMPLE_RATE = 44100
CHANNELS = 1
DURATION_S = 10
OGG_BITRATE = "64k"

# Stable archive.org URLs for source audio
# LibriVox recordings are public domain
SOURCES: dict[str, dict[str, str]] = {
    "voice_male": {
        "url": "https://archive.org/download/adventures_sherlock_holmes_0711_librivox/adventuresofsherlockholmes_01_doyle_64kb.mp3",
        "start": "30",  # Skip intro silence
        "desc": "Male narrator, LibriVox public domain",
    },
    "voice_female": {
        "url": "https://archive.org/download/jane_eyre_0810_librivox/janeeyre_01_bronte_64kb.mp3",
        "start": "45",  # Skip intro
        "desc": "Female narrator, LibriVox public domain",
    },
}

# Mixing matrix: output_name -> {stems, levels}
# Levels are in dB relative to normalized stems
MIXES: dict[str, dict[str, float]] = {
    "vocal_solo": {"voice_male": 0.0},
    "guitar_solo": {"guitar_acoustic": 0.0},
    "drums_solo": {"drums_jazz": 0.0},
    "backing_only": {"guitar_acoustic": 0.0, "drums_jazz": 0.0},
    "mix_vocal_loud": {"voice_male": 6.0, "guitar_acoustic": 0.0, "drums_jazz": 0.0},
    "mix_vocal_equal": {"voice_male": 0.0, "guitar_acoustic": 0.0, "drums_jazz": 0.0},
    "mix_vocal_quiet": {"voice_male": -6.0, "guitar_acoustic": 0.0, "drums_jazz": 0.0},
    "mix_vocal_buried": {
        "voice_male": -12.0,
        "guitar_acoustic": 0.0,
        "drums_jazz": 0.0,
    },
}


def find_ffmpeg() -> str:
    """Find ffmpeg binary, preferring static-ffmpeg from pip."""
    # Try static-ffmpeg first
    try:
        import static_ffmpeg  # pyright: ignore[reportMissingTypeStubs]

        static_ffmpeg.add_paths()  # pyright: ignore[reportUnknownMemberType]
    except ImportError:
        pass

    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg:
        return ffmpeg

    # Fallback: common locations
    for path in [
        r"C:\Users\niteris\bin\ffmpeg.EXE",
        "/usr/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
    ]:
        if os.path.isfile(path):
            return path

    print(
        "ERROR: ffmpeg not found. Install with: pip install static-ffmpeg",
        file=sys.stderr,
    )
    sys.exit(1)


def run_ffmpeg(ffmpeg: str, args: list[str], desc: str = "") -> None:
    """Run ffmpeg with error handling."""
    cmd = [ffmpeg, "-y", "-hide_banner", "-loglevel", "error", *args]
    if desc:
        print(f"  {desc}...")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  FAILED: {' '.join(cmd)}", file=sys.stderr)
        print(f"  stderr: {result.stderr}", file=sys.stderr)
        raise RuntimeError(f"ffmpeg failed: {result.stderr[:200]}")


def download_and_trim(
    ffmpeg: str, url: str, start: str, output: Path, cache_dir: Path
) -> bool:
    """Download audio from URL, trim to DURATION_S, normalize, resample to OGG."""
    # Cache the raw download
    url_hash = hashlib.md5(url.encode()).hexdigest()[:12]
    cached = cache_dir / f"raw_{url_hash}.mp3"

    if not cached.exists():
        print(f"  Downloading {url[:80]}...")
        try:
            import urllib.request

            urllib.request.urlretrieve(url, str(cached))
        except KeyboardInterrupt as ki:
            import _thread

            _thread.interrupt_main()
            raise SystemExit(1) from ki
        except Exception as e:
            print(f"  Download failed: {e}", file=sys.stderr)
            return False

    # Trim + normalize + resample + encode
    run_ffmpeg(
        ffmpeg,
        [
            "-ss",
            start,
            "-t",
            str(DURATION_S),
            "-i",
            str(cached),
            "-af",
            f"loudnorm=I=-10:LRA=7:TP=-1,aresample={SAMPLE_RATE}",
            "-ac",
            str(CHANNELS),
            "-c:a",
            "libvorbis",
            "-b:a",
            OGG_BITRATE,
            str(output),
        ],
        f"Processing -> {output.name}",
    )
    return True


def generate_synthetic_stem(ffmpeg: str, stem_type: str, output: Path) -> None:
    """Generate a synthetic audio stem using ffmpeg's signal generators."""
    if stem_type == "voice_male":
        # Synthetic male voice: F0=130Hz with formants at F1=700Hz, F2=1200Hz
        # Uses multiple harmonics shaped by formant envelopes
        expr_parts: list[str] = []
        f0 = 130.0
        for h in range(1, 30):
            freq = f0 * h
            if freq > SAMPLE_RATE / 2:
                break
            # Natural 1/h rolloff
            amp = 1.0 / h
            # Gaussian formant shaping (F1=700, F2=1200, F3=2600)
            import math

            f1_gain = math.exp(-0.5 * ((freq - 700) / 150) ** 2)
            f2_gain = math.exp(-0.5 * ((freq - 1200) / 200) ** 2)
            f3_gain = math.exp(-0.5 * ((freq - 2600) / 250) ** 2)
            formant = max(f1_gain, f2_gain, f3_gain, 0.02)
            final_amp = amp * formant * 0.3
            expr_parts.append(f"{final_amp:.4f}*sin(2*PI*{freq:.1f}*t)")
        expr = "+".join(expr_parts)
        # Add slight amplitude modulation for naturalness
        expr = f"({expr})*( 0.8 + 0.2*sin(2*PI*5.5*t) )"
        run_ffmpeg(
            ffmpeg,
            [
                "-f",
                "lavfi",
                "-i",
                f"aevalsrc={expr}:s={SAMPLE_RATE}:d={DURATION_S}",
                "-ac",
                str(CHANNELS),
                "-c:a",
                "libvorbis",
                "-b:a",
                OGG_BITRATE,
                str(output),
            ],
            f"Synthesizing {stem_type}",
        )

    elif stem_type == "voice_female":
        # Synthetic female voice: F0=220Hz with formants at F1=800, F2=1500
        expr_parts: list[str] = []
        f0 = 220.0
        for h in range(1, 20):
            freq = f0 * h
            if freq > SAMPLE_RATE / 2:
                break
            amp = 1.0 / h
            import math

            f1_gain = math.exp(-0.5 * ((freq - 800) / 160) ** 2)
            f2_gain = math.exp(-0.5 * ((freq - 1500) / 220) ** 2)
            f3_gain = math.exp(-0.5 * ((freq - 2800) / 280) ** 2)
            formant = max(f1_gain, f2_gain, f3_gain, 0.02)
            final_amp = amp * formant * 0.3
            expr_parts.append(f"{final_amp:.4f}*sin(2*PI*{freq:.1f}*t)")
        expr = "+".join(expr_parts)
        expr = f"({expr})*( 0.8 + 0.2*sin(2*PI*6.2*t) )"
        run_ffmpeg(
            ffmpeg,
            [
                "-f",
                "lavfi",
                "-i",
                f"aevalsrc={expr}:s={SAMPLE_RATE}:d={DURATION_S}",
                "-ac",
                str(CHANNELS),
                "-c:a",
                "libvorbis",
                "-b:a",
                OGG_BITRATE,
                str(output),
            ],
            f"Synthesizing {stem_type}",
        )

    elif stem_type == "guitar_acoustic":
        # Acoustic guitar: fundamental at 196Hz (G3) with body resonances
        expr_parts: list[str] = []
        f0 = 196.0
        for h in range(1, 25):
            freq = f0 * h
            if freq > 6000:
                break
            import math

            amp = 0.15 / (h**1.3)
            # Body resonances at 300, 800, 1500 Hz
            r1 = math.exp(-0.5 * ((freq - 300) / 120) ** 2)
            r2 = 0.6 * math.exp(-0.5 * ((freq - 800) / 200) ** 2)
            r3 = 0.3 * math.exp(-0.5 * ((freq - 1500) / 300) ** 2)
            body = max(r1, r2, r3, 0.05)
            final_amp = amp * body
            expr_parts.append(f"{final_amp:.4f}*sin(2*PI*{freq:.1f}*t)")
        expr = "+".join(expr_parts)
        # Pluck pattern: repeating amplitude envelope at ~2Hz
        expr = f"({expr})*( 0.5 + 0.5*abs(sin(PI*2*t)) )"
        run_ffmpeg(
            ffmpeg,
            [
                "-f",
                "lavfi",
                "-i",
                f"aevalsrc={expr}:s={SAMPLE_RATE}:d={DURATION_S}",
                "-ac",
                str(CHANNELS),
                "-c:a",
                "libvorbis",
                "-b:a",
                OGG_BITRATE,
                str(output),
            ],
            f"Synthesizing {stem_type}",
        )

    elif stem_type == "drums_jazz":
        # Use existing jazzy_percussion.mp3 if available, else synthesize
        existing_mp3 = Path("tests/data/codec/jazzy_percussion.mp3")
        if existing_mp3.exists():
            # Loop the 4s jazz beat to fill DURATION_S
            run_ffmpeg(
                ffmpeg,
                [
                    "-stream_loop",
                    "-1",
                    "-t",
                    str(DURATION_S),
                    "-i",
                    str(existing_mp3),
                    "-af",
                    f"loudnorm=I=-10:LRA=7:TP=-1,aresample={SAMPLE_RATE}",
                    "-ac",
                    str(CHANNELS),
                    "-c:a",
                    "libvorbis",
                    "-b:a",
                    OGG_BITRATE,
                    str(output),
                ],
                f"Looping jazzy_percussion.mp3 -> {stem_type}",
            )
        else:
            # Synthesize a basic beat: kick at 1Hz + snare noise bursts
            expr = (
                "0.7*exp(-mod(t,1)/0.03)*sin(2*PI*60*t)"  # Kick body
                "+0.3*exp(-mod(t,1)/0.03)*sin(2*PI*120*t)"  # Kick harmonic
                "+0.4*exp(-mod(t+0.5,1)/0.05)*(2*random(0)-1)"  # Snare noise
            )
            run_ffmpeg(
                ffmpeg,
                [
                    "-f",
                    "lavfi",
                    "-i",
                    f"aevalsrc={expr}:s={SAMPLE_RATE}:d={DURATION_S}",
                    "-ac",
                    str(CHANNELS),
                    "-c:a",
                    "libvorbis",
                    "-b:a",
                    OGG_BITRATE,
                    str(output),
                ],
                f"Synthesizing {stem_type}",
            )
    else:
        raise ValueError(f"Unknown stem type: {stem_type}")


def create_mix(
    ffmpeg: str,
    mix_name: str,
    stem_levels: dict[str, float],
    stems_dir: Path,
    output: Path,
) -> None:
    """Mix stems at specified dB levels into a single OGG file."""
    inputs: list[str] = []
    filter_parts: list[str] = []

    for i, (stem, db_level) in enumerate(stem_levels.items()):
        stem_path = stems_dir / f"{stem}.ogg"
        if not stem_path.exists():
            print(f"  WARNING: Stem {stem_path} not found, skipping mix {mix_name}")
            return
        inputs.extend(["-i", str(stem_path)])
        # Convert dB to linear gain
        import math

        gain = math.pow(10.0, db_level / 20.0)
        filter_parts.append(f"[{i}:a]volume={gain:.4f}[s{i}]")

    n = len(stem_levels)
    if n == 1:
        # Single stem, just copy with volume
        stem_name, db_level = next(iter(stem_levels.items()))
        stem_path = stems_dir / f"{stem_name}.ogg"
        import math

        gain = math.pow(10.0, db_level / 20.0)
        run_ffmpeg(
            ffmpeg,
            [
                "-i",
                str(stem_path),
                "-af",
                f"volume={gain:.4f}",
                "-c:a",
                "libvorbis",
                "-b:a",
                OGG_BITRATE,
                str(output),
            ],
            f"Creating {mix_name}",
        )
    else:
        # Multi-stem mix
        mixer_inputs = "".join(f"[s{i}]" for i in range(n))
        filter_complex = (
            ";".join(filter_parts)
            + f";{mixer_inputs}amix=inputs={n}:duration=longest[out]"
        )
        run_ffmpeg(
            ffmpeg,
            [
                *inputs,
                "-filter_complex",
                filter_complex,
                "-map",
                "[out]",
                "-c:a",
                "libvorbis",
                "-b:a",
                OGG_BITRATE,
                str(output),
            ],
            f"Mixing {mix_name} ({n} stems)",
        )


def verify_ogg(ffmpeg: str, path: Path) -> dict[str, str | int | float]:  # noqa: DCT001
    """Verify OGG file properties using ffprobe."""
    ffprobe = shutil.which("ffprobe")
    if not ffprobe:
        # Try alongside ffmpeg
        ffprobe_path = Path(ffmpeg).parent / "ffprobe"
        if ffprobe_path.exists():
            ffprobe = str(ffprobe_path)
        else:
            ffprobe_path = Path(ffmpeg).parent / "ffprobe.exe"
            if ffprobe_path.exists():
                ffprobe = str(ffprobe_path)
    if not ffprobe:
        return {"error": "ffprobe not found"}

    result = subprocess.run(
        [
            ffprobe,
            "-v",
            "quiet",
            "-print_format",
            "json",
            "-show_streams",
            str(path),
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return {"error": result.stderr[:200]}

    import json

    info = json.loads(result.stdout)
    streams = info.get("streams", [])
    if not streams:
        return {"error": "no streams"}
    s = streams[0]
    return {
        "codec": s.get("codec_name", "?"),
        "sample_rate": int(s.get("sample_rate", 0)),
        "channels": int(s.get("channels", 0)),
        "duration": float(s.get("duration", 0)),
        "size_kb": path.stat().st_size // 1024,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate audio fixtures for VocalDetector tests"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("tests/data/audio"),
        help="Output directory (default: tests/data/audio)",
    )
    parser.add_argument(
        "--synth-only",
        action="store_true",
        help="Use synthetic generation only (no downloads)",
    )
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=Path(tempfile.gettempdir()) / "fastled_audio_cache",
        help="Cache directory for downloads",
    )
    args = parser.parse_args()

    ffmpeg = find_ffmpeg()
    print(f"Using ffmpeg: {ffmpeg}")

    # Create output directories
    stems_dir = args.output / "stems"
    mixes_dir = args.output / "mixes"
    stems_dir.mkdir(parents=True, exist_ok=True)
    mixes_dir.mkdir(parents=True, exist_ok=True)
    args.cache_dir.mkdir(parents=True, exist_ok=True)

    # --- Generate stems ---
    print("\n=== Generating stems ===")
    all_stems = ["voice_male", "voice_female", "guitar_acoustic", "drums_jazz"]

    for stem_name in all_stems:
        output_path = stems_dir / f"{stem_name}.ogg"
        if output_path.exists():
            print(
                f"  {stem_name}: already exists ({output_path.stat().st_size // 1024}KB)"
            )
            continue

        downloaded = False
        if not args.synth_only and stem_name in SOURCES:
            src = SOURCES[stem_name]
            downloaded = download_and_trim(
                ffmpeg, src["url"], src["start"], output_path, args.cache_dir
            )

        if not downloaded:
            print(f"  {stem_name}: using synthetic fallback")
            generate_synthetic_stem(ffmpeg, stem_name, output_path)

    # --- Generate mixes ---
    print("\n=== Generating mixes ===")
    for mix_name, stem_levels in MIXES.items():
        output_path = mixes_dir / f"{mix_name}.ogg"
        if output_path.exists():
            print(
                f"  {mix_name}: already exists ({output_path.stat().st_size // 1024}KB)"
            )
            continue
        create_mix(ffmpeg, mix_name, stem_levels, stems_dir, output_path)

    # --- Verify all files ---
    print("\n=== Verification ===")
    total_size = 0
    all_ok = True
    for subdir in [stems_dir, mixes_dir]:
        for ogg_file in sorted(subdir.glob("*.ogg")):
            info = verify_ogg(ffmpeg, ogg_file)
            size_kb = ogg_file.stat().st_size // 1024
            total_size += size_kb
            if "error" in info:
                print(f"  FAIL {ogg_file.name}: {info['error']}")
                all_ok = False
            else:
                sr = info.get("sample_rate", "?")
                ch = info.get("channels", "?")
                dur = float(info.get("duration", 0))
                print(f"  OK   {ogg_file.name}: {sr}Hz {ch}ch {dur:.1f}s {size_kb}KB")
                if sr != SAMPLE_RATE:
                    print(f"       WARNING: expected {SAMPLE_RATE}Hz")
                    all_ok = False
                if ch != CHANNELS:
                    print(f"       WARNING: expected {CHANNELS} channel(s)")
                    all_ok = False

    print(f"\nTotal size: {total_size}KB")
    if all_ok:
        print("All fixtures generated successfully.")
    else:
        print("Some fixtures had issues - check warnings above.")
        sys.exit(1)


if __name__ == "__main__":
    main()
