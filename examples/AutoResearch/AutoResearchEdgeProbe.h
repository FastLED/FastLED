// AutoResearch edge probe (FastLED#3586 bring-up tooling).
//
// A GPIO any-edge ISR that timestamps up to 200 edges with the CPU
// cycle counter — an in-firmware logic probe for "the driver transmits
// but the capture backend records nothing" investigations. Armed via
// the edgeProbe RPC, read back via edgeProbeRead.

#pragma once

#include "fl/stl/int.h"

/// Arm the probe on a pin (re-arming resets the buffer).
void edgeProbeArm(int pin);

/// Passive variant: no pinMode/attachInterrupt/pull toggles — only the
/// triggered tight-loop sampler task. Safe on pads owned by a receiver.
void edgeProbeArmMode(int pin, bool passive);

/// Timestamp buffer: entry = (level_before_edge << 31) | cycle_count.
/// Returns the buffer; count receives the number of valid entries.
const fl::u32 *edgeProbeStamps(fl::u32 &count);

/// CPU frequency in MHz for cycle→ns conversion.
fl::u32 edgeProbeCpuMhz();

/// Edges recorded by the arm-time pull-toggle self-test.
fl::u32 edgeProbeSelfTestEdges();

/// Probe task progress: 0=not started 1=started 2=triggered 3=sampled.
fl::u32 edgeProbeState();

/// RMT registers snapshotted at trigger time (INT_RAW, CH2STATUS, CH3STATUS, CH2_RX_CONF1).
const fl::u32 *edgeProbeRmtLive();
