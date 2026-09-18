# MoodRing redesign journal

Short record of why the sketch was rebuilt and what each round changed.
Lineage: AnimartrixRing (#2713, #2809) -> MoodRing split -> VisualControlBus
and pulse overlay (#3885) -> this rebuild. Design thread: #2256.

## What MoodRing is for

A ring that turns the sound of a room into light, honestly. Three regimes:

- a quiet room should look intentional, not idle
- a busy, non-musical room should look alive without faking a beat
- music with a beat should hit on the beat, and downbeats should hit harder

Transitions between regimes should be invisible. Nothing should flicker or
cut. The room is the input, not "music".

## Why the first version was half-broken

1. The bus derived nine signals and the engine consumed one (speed). Eight
   signals were dead weight: computed, printed, never seen on the ring.
2. Animation banks cycled on an 18 s wall clock with a hard `fxSet` cut.
   Every cycle was a visual cliff unrelated to the audio.
3. Three exclusive states plus dwell plus hysteresis meant three timers and
   a chatter-tuning problem. Every state switch was also a visual cliff.
4. A 16x16 Animartrix grid was rendered so the ring could sample its
   circumference: 256 pixels of work for 244, and 2D visuals do not read as
   ring-native.
5. The overlay was engine-agnostic but there was no second engine, so the
   abstraction bought nothing.

## Round 1: listen / paint split, continuous blend

Two halves with one struct between them.

- `RoomListener` polls the audio Processor and fills `RoomSense`: smoothed
  0..1 signals (presence, energy, trend, punch, shimmer, bands, groove
  confidence, beat phase, warmth, arousal) and a one-frame event lane.
- The three regimes become three weights that sum to one: `calm`, `flow`,
  `groove`. They are derived from two smoothed signals (presence and groove
  confidence) with asymmetric attack/release. No state enum, no dwell
  timers, no hysteresis code. Chatter cannot happen because the weights
  cannot jump.
- `MoodPainter` renders three ring-native layers into scratch buffers and
  mixes them by weight. Cross-fade is a property of the mix, not a feature
  someone has to write.
- Layers draw straight onto the 1D ring. No 2D grid, no Animartrix.

## Round 2: make time musical

- Groove layer anticipates the beat: brightness swells into the beat using
  beat phase, then releases. Hits land on the beat instead of after it.
- Rising energy reads differently from falling energy: `trend` is a fast
  energy follower minus a slow one. Flow lobes widen while rising and
  tighten while falling.
- Downbeats spawn a wider, warmer pulse ring than ordinary beats. Pulses
  expand from a configurable origin so a mounted ring can point them up.
- Palette comes from mood: valence chooses warm vs cool hue centre, arousal
  chooses saturation and hue spread. Silence drifts the hue slowly so a
  long quiet stretch still moves.

## Round 3: simplify and harden

- Every tunable that reaches the UI lives in one `Tuning` struct per half.
  Anything not in a struct is a named constant next to its use.
- Everything renders and mixes in float and is quantised to CRGB exactly
  once. Repeated 8-bit scaling was the source of the stair steps seen
  before: a dim tail going 3 -> 2 -> 1 -> 0, and layer weights stepping in
  1/255 increments between frames.
- Every hit goes through a Follower with a short attack. Sparkle is a pool
  of grains with attack/decay envelopes, not per-frame random pops. Groove
  markers rotate continuously at tempo and slew a phase correction on each
  beat instead of snapping to it.
- Trails keep the unity-gain max-persistence trick from the old overlay,
  so a static frame is exactly itself and nothing can clip to white.
- Scratch buffers are sized once on first draw. No per-frame allocation.
- Sparkle uses a tiny xorshift so the treble shimmer is deterministic and
  free of libc.
- The Listener derives beat phase from its own beat timestamps and the
  reported BPM. It does not depend on which Processor callbacks fire.
- One debug line prints the whole `RoomSense` at 2 Hz so the classifier is
  observable in the field without a screen.

## File map

| File | Role |
|------|------|
| `MoodRing.ino` | Wiring, UI, loop |
| `room_sense.{h,cpp}` | `RoomSense` and `RoomListener`: audio in, signals out |
| `mood_palette.{h,cpp}` | Mood to colour |
| `ring_canvas.{h,cpp}` | Ring geometry and additive drawing helpers |
| `layers.{h,cpp}` | Breath, Flow, Groove layers |
| `mood_painter.{h,cpp}` | Layer mix, trails, pulses |
| `ring_screenmap.{h,cpp}` | Circular ScreenMap for the web preview |
| `auto_brightness.{h,cpp}` | Content-aware brightness compression |
