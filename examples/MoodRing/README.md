# MoodRing

**A 244-LED ring that listens to the room and lights the room back.**

The input is not "music". It is the room, whatever it sounds like:
conversation, silence, laughter, clatter, a song coming on. MoodRing senses
the room and paints a light response that cross-fades between three looks as
the room changes. Design thread: [issue #2256](https://github.com/FastLED/FastLED/issues/2256).
Round-by-round design notes: [`DESIGN.md`](DESIGN.md).

## Architecture

```text
Mic -> fl::audio::Processor
    -> RoomListener   fills RoomSense: smooth signals + calm/flow/groove weights
    -> MoodPainter    Breath, Flow, Groove layers mixed by weight, then trails + pulses
    -> ring
```

There is no state machine. `calm`, `flow` and `groove` are three weights that
always sum to one, derived from two smoothed signals (presence and groove
confidence) with asymmetric attack and release. The ring wakes on the first
sound, settles slowly into calm, and never cuts between looks because the mix
is continuous.

### The three looks

| Weight | When | Layer |
|--------|------|-------|
| `calm` | no sound present | **Breath**: whole-ring breath plus two slow soft blobs drifting in opposite directions. Audio does not drive it; silence looks intentional. |
| `flow` | sound present, no trustworthy beat | **Flow**: bass sets the size of two counter-rotating lobes, mids set their speed, treble scatters sparkle grains. Rising energy widens everything; falling energy tightens it. |
| `groove` | tempo and beat confidence both high | **Groove**: four markers step one slot per beat, the ring swells *into* each beat and releases after it, snares add thin counter-markers, downbeats flash the ring and spawn a wider pulse. |

Colour comes from mood: valence picks the hue centre (cool for sad or tense,
warm for happy), arousal picks saturation and hue spread.

## Files

| File | Purpose |
|------|---------|
| `MoodRing.ino` | Wiring, UI, loop |
| `room_sense.{h,cpp}` | `RoomSense` and `RoomListener`: audio in, signals out |
| `mood_palette.{h,cpp}` | Mood to colour family |
| `ring_canvas.{h,cpp}` | Ring geometry and additive drawing helpers |
| `layers.{h,cpp}` | Breath, Flow and Groove layers |
| `mood_painter.{h,cpp}` | Layer mix, trails, beat pulses |
| `ring_screenmap.{h,cpp}` | Circular `ScreenMap` for the web preview |
| `auto_brightness.{h,cpp}` | Content-aware brightness compression |

## Run it

```bash
pip install fastled
cd examples/MoodRing
fastled
```

Grant microphone access when the browser asks, or drag a `.wav` onto the page.
Turn on **Debug: Print Sense** to watch the signals and weights on the
serial console at 2 Hz.
