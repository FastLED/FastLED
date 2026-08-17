# MoodRing

**A 244-LED ring that listens to the room and lights the room back.**

The input is not "music" — it is the room, whatever it sounds like:
conversation, silence, laughter, clatter, a song coming on. MoodRing classifies
the sound environment and paints a light response to it. This is the
product-track sketch; [`examples/AnimartrixRing/`](../AnimartrixRing/) is the
minimal technique demo it is built on top of.

Design doc and roadmap: [issue #2256](https://github.com/FastLED/FastLED/issues/2256).

## Architecture

```text
Mic -> fl::audio::Processor      silence / energy / eq / tempo / beat / percussion
    -> SoundOrchestrator         Silence | Disorganized | BpmLocked
    -> per-state Animartrix bank calm ambient / spectrum / beat geometry
    -> ring
```

### The three states

| State | Entry condition | Visual strategy |
|-------|-----------------|-----------------|
| `Silence` | `isSilent()` held for `silenceEnterMs` | Calm ambient: `SLOW_FADE`, `WATER`, `PARAMETRIC_WATER`, `FLUFFY_BLOBS`. Low speed, restrained brightness, no hard flashes. |
| `Disorganized` | Sound present, no trustworthy tempo | Energy/spectrum: `RGB_BLOBS5`, `WAVES`, `POLAR_WAVES`, `COMPLEX_KALEIDO`. Bass nudges speed within a bounded span — time warp is a *secondary* accent, not the whole model. |
| `BpmLocked` | Tempo **and** beat confidence above threshold | Beat geometry: `RINGS`, `CHASING_SPIRALS`, `SPIRALUS`, `CENTER_FIELD`. Kick/snare/downbeat emit decaying pulses; BPM sets cadence; measure phase fills the gap between beats. |

Transitions require both a minimum dwell in the current state and the candidate
state's entry condition holding continuously, so the classifier does not chatter
on borderline audio. `Silence` gets asymmetric hysteresis: slow to enter, fast to
leave, so a single sample wakes the ring but a brief gap mid-song does not drop
it into ambient mode.

## Files

| File | Purpose |
|------|---------|
| `MoodRing.ino` | Wiring: audio input, orchestrator, ring rendering, UI |
| `sound_orchestrator.{h,cpp}` | The 3-state classifier and animation-bank selection |
| `visual_control_bus.{h,cpp}` | `SoundState`, the derived-signal bus, and its per-state policy |
| `ring_overlay.{h,cpp}` | Post-process passes on the ring: trails and pulse rings |
| `ring_screenmap.{h,cpp}` | Builds the circular `ScreenMap` inside a rectangular grid |
| `auto_brightness.{h,cpp}` | Content-aware brightness compression |

## Run it

```bash
pip install fastled
cd examples/MoodRing
fastled
```

Grant microphone access when the browser asks, or drag a `.wav` onto the page.

## Roadmap

Landed:

- 3-state classifier with hysteresis and minimum dwell (#2713, PR #2809)
- Per-state Animartrix visual banks and per-state audio→visual mapping
- **VisualControlBus** (#3885) — one struct of derived signals
  (`transportSpeed`, `radialPressure`, `rotationBias`, `paletteDrift`,
  `sparkleDensity`, `decayAmount`, band levels, `pulseStrength`) that every
  engine consumes. Keyed on `max(0, vibeBass - vibeBassAtt)`, so a transient
  reads as a hit while sustained loudness does not.
- **Overlay compositor** (#3885) — trails and expanding pulse rings on the 1D
  ring buffer after the engine writes it, so it costs one implementation rather
  than one per engine

Still to land — see [#2256](https://github.com/FastLED/FastLED/issues/2256) for
the full design and PR split:

- **Overlay: sector emphasis and sparkle** — the remaining two passes. The bus
  already carries `lowBand`/`midBand`/`highBand` and `sparkleDensity`, so they
  attach without reopening the derivation
- **Mood-quadrant bias** — `MoodAnalyzer` valence × arousal steering palette
  family and motion character
- **NoiseRing engine** — a second, cheaper engine behind the same bus, with
  cross-fade on switch
- **Patch schema** — JSON ⇄ patch struct, URL serialization, curated presets
  promoted from playground sessions to firmware
