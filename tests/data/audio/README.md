# Audio Test Fixtures

OGG Vorbis files for VocalDetector integration tests.

## Regeneration

```bash
uv run python ci/tools/generate_audio_fixtures.py              # Download + process
uv run python ci/tools/generate_audio_fixtures.py --synth-only  # Synthetic only (offline)
```

## Stems (tests/data/audio/stems/)

| File | Source | License |
|------|--------|---------|
| voice_male.ogg | Synthetic (F0=130Hz, formants at 700/1200/2600Hz) or LibriVox public domain | PD |
| voice_female.ogg | Synthetic (F0=220Hz, formants at 800/1500/2800Hz) or LibriVox public domain | PD |
| guitar_acoustic.ogg | Synthetic (F0=196Hz, body resonances at 300/800/1500Hz) | N/A |
| drums_jazz.ogg | Looped from tests/data/codec/jazzy_percussion.mp3 or synthetic | See codec dir |

All stems: 10 seconds, 44100Hz, mono, 64kbps OGG Vorbis.

## Mixes (tests/data/audio/mixes/)

| File | Contents | Voice Level |
|------|----------|-------------|
| vocal_solo.ogg | Voice only | 0dB |
| guitar_solo.ogg | Guitar only | - |
| drums_solo.ogg | Drums only | - |
| backing_only.ogg | Guitar + Drums | - |
| mix_vocal_loud.ogg | Voice + Guitar + Drums | +6dB vs backing |
| mix_vocal_equal.ogg | Voice + Guitar + Drums | 0dB vs backing |
| mix_vocal_quiet.ogg | Voice + Guitar + Drums | -6dB vs backing |
| mix_vocal_buried.ogg | Voice + Guitar + Drums | -12dB vs backing |

## Observed Detection (synthetic stems, 3s window)

| Fixture | avgConf | vocalRate | isVocal |
|---------|---------|-----------|---------|
| vocal_solo | 0.59 | 52% | YES |
| guitar_solo | 0.45 | 0% | no |
| drums_solo | 0.26 | 0% | no |
| backing_only | 0.33 | 0% | no |
| mix_vocal_loud | 0.34 | 0% | no |
| mix_vocal_equal | 0.32 | 0% | no |
| mix_vocal_quiet | 0.32 | 0% | no |

## Test Assertions

| Test | Assertion |
|------|-----------|
| vocal_solo | vocalRate >= 40%, avgConf >= 0.35 |
| mix_vocal_loud | avgConf >= backing avgConf - 0.02 |
| guitar_solo | vocalRate <= 25%, avgConf < 0.55 |
| drums_solo | vocalRate <= 25% |
| backing_only | vocalRate <= 30% |
| gradient | loud >= equal >= quiet (with 0.05 tolerance) |
| stem mixing | voice+guitar avgConf > guitar avgConf - 0.05 |

## Feature Diagnostics (last-frame values, 2s window)

| Fixture | conf | flat | form | pres | zcCV | vocal |
|---------|------|------|------|------|------|-------|
| vocal_solo | 0.86 | 0.62 | 0.53 | 0.06 | 0.51 | YES |
| guitar_solo | 0.46 | 0.34 | 0.17 | 0.00 | 0.05 | no |
| drums_solo | 0.40 | 0.73 | 1.47 | 2.38 | 0.80 | no |
| backing_only | 0.33 | 0.74 | 1.18 | 0.55 | 0.99 | no |
