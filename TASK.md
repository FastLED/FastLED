Auto‑Tuning Extension for FastLED Sound_to_MIDI
1 Introduction and problem statement

FastLED's sound_to_midi module converts short audio frames into MIDI Note On/Off events in either a monophonic mode (using a YIN/MPM style autocorrelation detector) or a polyphonic mode (spectral‐peak detection with harmonic filtering). Both paths expose configurable thresholds, smoothing options and hold‑times via the SoundToMIDI configuration structure. In practice, users report that the default settings either let through many spurious MIDI events when the ambient noise is high or miss notes when the signal is quiet. Manual tuning—adjusting RMS gates, confidence thresholds, peak thresholds and smoothing parameters—helps but is time‑consuming and brittle; the optimal settings vary with environment, instrument and sample rate. The goal of this work is to design an automatic tuning mechanism that continuously adapts detection thresholds and filters as audio is processed, reducing false triggers while maintaining responsiveness and low latency.

2 Goals and non‑goals

Goals

Reduce false MIDI events and jitter by automatically adjusting pitch‑detection parameters based on real‑time measurements.

Retain FastLED's ability to run on low‑resource microcontrollers (ESP32, SAMD), with minimal additional CPU and memory overhead.

Maintain backward compatibility: existing applications should work without modification when auto‑tuning is disabled.

Provide API hooks for users to monitor and tweak auto‑tuning behaviour (e.g. set limits or turn on logging).

Non‑goals

Introducing heavy DSP or machine‑learning algorithms that exceed embedded resource budgets.

Changing the underlying pitch‑detection algorithms (YIN/MPM or spectral peak); we adapt the thresholds, not the core detection logic.

Persisting per‑instrument calibration across sessions—auto‑tuning is session‑based.

3 Overview of tunable parameters

The existing SoundToMIDI struct defines numerous knobs:

Monophonic detection:

confidence_threshold (0–1) – minimum pitch confidence.

note_hold_frames – number of consecutive frames before a note on is emitted.

silence_frames_off – consecutive silent frames before note off.

rms_gate – RMS amplitude threshold below which a frame is considered silent.

note_change_semitone_threshold, note_change_hold_frames and median_filter_size – parameters to filter out small pitch jitters.

Polyphonic detection:

window_type, spectral_tilt_db_per_decade and smoothing_mode – spectral conditioning options.

peak_threshold_db – magnitude threshold (in dB) for spectral peaks.

parabolic_interp – sub‑bin interpolation toggle.

harmonic_filter_enable, harmonic_tolerance_cents, harmonic_energy_ratio_max – parameters for harmonic suppression.

octave_mask – bitmask enabling or disabling detection in specific octaves.

pcp_enable, pcp_history_frames, pcp_bias_weight – pitch‑class profile stabiliser settings.

These parameters are currently set once at startup. The auto‑tuning extension will adjust them dynamically.

4 Conceptual approach

The auto‑tuning system treats the pitch‑detection engine as a feedback‑controlled process. It measures the noise floor, pitch confidence, spectral occupancy and event rate from recent frames, then uses heuristics to adjust thresholds, smoothing and gating parameters. Key components include:

Noise floor estimator – computes an exponential moving average (EMA) of RMS amplitudes and spectral magnitudes when the engine believes there is no note present. It tracks slow changes in ambient noise.

Adaptive gates – sets the rms_gate and peak_threshold_db relative to the estimated noise floor. For example, the RMS gate becomes noise_rms × k_rms and the peak threshold becomes noise_mag_db + k_peak, where k_rms and k_peak are tunable margins.

Confidence tracker – monitors the distribution of detection confidences (for monophonic) or the number of peaks surviving filtering (for polyphonic). If too many low‑confidence detections occur, the confidence_threshold or peak_threshold_db is increased slightly; if genuine notes are missed, the thresholds are lowered.

Hold‑time optimiser – adjusts note_hold_frames and silence_frames_off according to observed note durations and inter‑note gaps. It uses running histograms of durations to choose hold lengths that are long enough to suppress noise bursts but short enough to preserve responsiveness.

Jitter monitor – evaluates variability in consecutive pitch estimates. High jitter suggests noise or unstable detection; the system can widen the median filter (median_filter_size) or increase note_change_hold_frames to smooth the output. Low jitter permits narrower filters to reduce latency.

Octave and harmonic adjuster (polyphonic) – maintains statistics about which octaves and harmonics are frequently triggered. If spurious notes cluster in certain octaves or are likely overtones, the system tightens the harmonic filter (reducing harmonic_energy_ratio_max or increasing harmonic_tolerance_cents) or disables those octaves via octave_mask. Conversely, if legitimate notes appear in a previously disabled octave, the mask can be re‑enabled.

These adjustments operate on a sliding window (e.g. the last 0.5–1 second of audio) and update parameters smoothly to avoid oscillations.

5 Detailed algorithm
5.1 Noise floor estimation

Maintain two EMAs: one for RMS amplitude (noise_rms_est) and one for spectral median dB (noise_mag_db_est). Update them only when no note is currently sounding (monophonic) or when the number of detected peaks is below a minimal threshold (polyphonic). Use a long time constant (e.g. 0.5 s) to track room noise but ignore transient signals.

5.2 Adaptive thresholds

RMS gate (rms_gate): Set to max(default_rms_gate, noise_rms_est × k_rms). A typical margin k_rms is 1.5–2.0. This prevents the gate from dropping too low in very quiet environments (avoiding false triggers) and from rising too high (missing quiet notes).

Peak threshold (peak_threshold_db): In polyphonic mode, compute the median (or mean) of the magnitude spectrum for each frame, convert to dB and track an EMA. Set the peak threshold to noise_mag_db_est + margin_db. A margin of 6–10 dB above the noise floor is a good starting point. Optionally adapt margin_db up or down based on the current event rate (Section 5.5).

5.3 Confidence and jitter adjustments

Confidence threshold (monophonic): Maintain an EMA of detected pitch confidences for note onsets. If the average confidence of recent note‐on events is below the current confidence_threshold minus a safety margin, raise confidence_threshold by a small step (e.g. +0.02) to reject spurious detections. If several frames pass with no note despite a moderately strong RMS, lower the threshold (e.g. −0.02) to avoid missing valid notes. Clamp within [0.6, 0.95] to avoid extremes.

Note change filter: Monitor the semitone difference between consecutive pitch estimates. If frequent ±1‑semitone fluctuations occur, increase note_change_semitone_threshold (to 2) or note_change_hold_frames (to 4–5). Conversely, if the detector lags behind legitimate pitch changes, reduce these values.

Median filter: Compute the median absolute deviation (MAD) of pitch estimates. If the MAD is high, enlarge the median_filter_size to 3 or 5; if low, reduce it to 1 to minimise latency.

5.4 Hold‑time optimisation

Track histograms of note durations (from note on to note off) and inter‑note gaps. Compute the median of each distribution. Set note_hold_frames slightly below the median note duration (e.g. 75th percentile of durations divided by frame hop) to ensure a note is recognised quickly. Set silence_frames_off slightly below the median gap duration. When the environment is noisy, increase both hold times modestly to avoid fluttering; when stable, decrease them for responsiveness.

5.5 Event rate control

Maintain a counter of note‑on events per second (monophonic) or peak detections per frame (polyphonic). Define target ranges—e.g. 1–10 notes per second for monophonic, 1–5 peaks per frame for polyphonic. If the rate exceeds the upper bound, increment peak_threshold_db and/or confidence_threshold gradually. If the rate falls below the lower bound and RMS suggests there should be notes, decrement those thresholds. This provides a high‑level feedback loop stabilising the number of events.

5.6 Harmonic and octave control (polyphonic)

Maintain counts of detected notes per octave. If an octave consistently produces spurious detections (e.g. low rumble or hiss), increase harmonic_energy_ratio_max from 0.7 to 0.5, or increase harmonic_tolerance_cents to 50–60 cents to allow more aggressive grouping; if it remains noisy, clear the corresponding bit in octave_mask. Conversely, if an octave has been disabled but the EMA of RMS energy in that frequency band rises significantly, re‑enable it.

Monitor the ratio of fundamental to overtone energy. If harmonic peaks regularly exceed this ratio, temporarily disable harmonic filtering (harmonic_filter_enable=false) to prevent legitimate notes from being removed; re‑enable once the detection stabilises.

5.7 PCP stabiliser tuning (polyphonic)

Measure the distribution of recently detected pitch classes. If the engine frequently oscillates between two pitch classes within the same octave, enable the PCP stabiliser (pcp_enable=true) and gradually increase pcp_bias_weight to up‑weight the persistent classes. If the pitch class distribution is stable, decrease pcp_bias_weight or disable PCP to reduce bias against novel notes.

5.8 Update schedule and smoothing

To avoid audible oscillation of parameters, update all adaptation variables at a fixed, moderate rate (e.g. 5–10 Hz). Each update applies an incremental change capped by a maximum delta per second (for thresholds and gates) to ensure smooth transitions. Use first‑order low‑pass filtering to blend new parameter values with the previous ones, preserving continuity.

6 Implementation
6.1 Data structures

Extend SoundToMIDI and the internal SoundToMIDIEngine with the following fields:

bool auto_tune_enable – master switch for auto‑tuning.

float noise_rms_est, float noise_mag_db_est – noise floor estimates.

EMAs for average pitch confidence, pitch variance, event rate, note durations and gaps.

Counters for harmonic peaks and octave statistics.

Configuration limits for each tunable parameter (min and max values, adaptation rate).

6.2 Algorithm integration

Initial calibration: On engine initialisation (or when auto_tune_enable is toggled), run a short calibration phase (0.5–1 s) where audio is analysed but no note events are emitted. Use this period to estimate initial noise floor, confidence distribution and typical event rates.

Modified processFrame: After each frame is processed by the existing pitch detector:

Determine whether a note is currently active or, in polyphonic mode, whether peaks exceed a minimal count. If not, update the noise floor estimates.

Update EMAs for confidence, pitch variance and event rate.

Every N frames (e.g. N = hop_size × update_frequency / frame_size), call an autoTuneUpdate() method that calculates new parameter values according to Section 5. This method applies smoothing and clamps to user‑defined ranges.

Use the updated parameters for subsequent frames.

Backward compatibility: When auto_tune_enable is false, the engine behaves exactly as before. Users can still manually set all thresholds.

API exposure: Add setters/getters to inspect current adaptive parameter values and to set adaptation margins (k_rms, margin_db, event rate bounds, adaptation speeds). Provide optional callbacks so applications can be notified when the auto‑tuner adjusts values.

6.3 Footprint considerations

The EMAs and counters require only a handful of floats/integers; memory overhead is negligible (< 1 KB).

Computation involves basic arithmetic, comparisons and occasional histogram updates. Running the adaptation logic at 5 Hz adds < 1 % CPU on an ESP32 (estimated at a few hundred operations per update). The heavy lifting (FFT, pitch detection) remains unchanged.

To conserve CPU, disable adaptation steps (harmonic control, PCP adjustment) if the corresponding features are off (e.g. when harmonic_filter_enable=false or pcp_enable=false).

7 Testing and tuning plan

To validate the auto‑tuning system:

Synthetic datasets: Generate audio files with known pitches and controlled noise (white, pink, environmental). Measure false‑positive and false‑negative rates as auto‑tuning adjusts thresholds. Verify that event rate stays within target bounds.

Real instruments: Test with monophonic sources (e.g. sine waves, flute) and polyphonic sources (piano chords, guitar strums) at various volumes. Record the raw audio and the MIDI output. Compare note timing, pitch accuracy and the number of spurious events.

Environmental noise: Introduce background sounds (talking, HVAC, traffic) and confirm that the noise floor adapts, raising thresholds to suppress false events without missing quieter notes.

Latency measurement: Measure end‑to‑end latency for note detection with and without auto‑tuning to ensure that adaptation and larger filters do not delay note onsets beyond acceptable limits (e.g. ≤ 50 ms for polyphonic and ≤ 30 ms for monophonic).

Stress test: Feed rapid sequences and dense chords to detect any oscillatory behaviour or instability in the adaptation loops; adjust smoothing constants if necessary.

Parameter sweep: Evaluate different values of adaptation margins (k_rms, margin_db), update frequencies and smoothing constants. Provide recommended presets for typical environments (quiet room, live stage, outdoor).

8 Potential extensions and future work

Machine learning‑based adaptation: Train a lightweight model on annotated audio to predict optimal thresholds based on spectral features. This could improve adaptation in complex, non‑stationary noise conditions while still fitting into embedded resources.

Per‑instrument profiles: Allow users to select instrument profiles (piano, guitar, voice) that bias the auto‑tuner's parameters (e.g. expected octaves and harmonic content).

Persistent calibration: Save noise floor estimates and parameter settings to non‑volatile storage to skip calibration on subsequent runs.

Graphical tuning interface: Provide a real‑time visualisation (e.g. on a web interface or over serial) showing current parameter values, noise floor and event rate, allowing advanced users to monitor and override auto‑tuning decisions.

9 Conclusion

FastLED's sound_to_midi already supplies a flexible set of parameters for pitch detection and MIDI conversion. However, these parameters are currently static and require manual tuning. The auto‑tuning extension proposed here introduces a feedback loop that continuously monitors the audio signal, estimates noise and event statistics, and adjusts thresholds, smoothing and gating accordingly. The design emphasises smooth, low‑overhead adaptation suitable for microcontrollers, backwards compatibility, and configurability. By implementing this design, FastLED can respond dynamically to changing acoustic environments, producing cleaner MIDI outputs without sacrificing responsiveness.
