# Driver Bring-up Postmortems

This guide turns hardware bring-up incidents into reusable engineering rules.
It is deliberately evidence-first: a successful build, a boot banner, and a
byte-exact loopback are different facts and must not be collapsed into one
claim.

## Source and command audit

Use these sources in order:

1. Current FastLED code and the [AutoResearch wrapper](../../autoresearch),
   [argument parser](../../ci/autoresearch/args.py), and
   [phase runner](../../ci/autoresearch/phases.py).
2. The [AutoResearch firmware harness](../../examples/AutoResearch) and the
   relevant driver implementation.
3. The project runbooks: [CLAUDE.md](../../CLAUDE.md) and
   [hardware AutoResearch guidance](hardware-autoresearch.md).
4. Hardware facts in [FastLED/datasheets](https://github.com/FastLED/datasheets)
   and production USB board identity in
   [FastLED/boards](https://github.com/FastLED/boards).
5. Linked issue, pull-request, commit, and board-run evidence.

The accepted live path is:

```bash
fbuild port scan
bash autoresearch <environment> --upload-port <port> --timeout 120s
```

For an external loopback, record the physical TX-to-RX jumper and add the
driver-specific flag, for example `--flex-io --tx-pin <tx> --rx-pin <rx>`.
Use the fbuild-backed AutoResearch JSON-RPC transport only. Do not substitute
PlatformIO, direct flashing tools, raw pyserial, or a hand-maintained VID/PID
table for this workflow.

## Evidence levels

| Evidence | Establishes | Does not establish |
| --- | --- | --- |
| Compile | The selected source/configuration compiles. | Board, transport, pins, peripheral activity, or protocol bytes. |
| Boot | The deployed image starts and emits its expected ready signal. | RPC handling, a GPIO connection, or a driver. |
| RPC | The fbuild-backed serial path can make and receive a structured request. | Physical wiring or signal timing. |
| GPIO | A requested TX GPIO reaches the requested RX GPIO in the GPIO pre-test. | Peripheral mux ownership, clockless waveform, or byte content. |
| Raw edge | A qualified RX backend observed transitions with usable durations. | Correct framing/decoding unless the decoder also passes. |
| Timing | Measured signal durations/cadence meet the declared tolerance. | Full-frame byte correctness or concurrent resource safety. |
| Byte exact | The RX oracle decoded the expected bytes for the declared frame. | An unobserved strip, another lane, or a different API path. |
| Wire idle | The line returns to the required idle/reset state. | A complete payload or a successful next frame. |
| Memory/resource | Claims, failures, cleanup, and recovery were observed under the stated pressure. | Long-duration timing stability without a soak. |
| Soak | The named matrix passed repeatedly for the stated count/duration. | Conditions outside that matrix. |

Treat a failed test as evidence too. A structured capacity error proves the
failure path only when it returns promptly and a same-session known-good
control succeeds afterward.

## Required incident row

Every postmortem incident uses this compact schema:

| Field | Required content |
| --- | --- |
| Sources | Direct issue/PR/commit links, plus current source paths where relevant. |
| False lead | The plausible but incorrect interpretation. |
| Discriminator | The smallest observation that separated the false lead from the cause. |
| Root cause | A narrow, testable causal statement. |
| Working fix | What was changed and which acceptance command proved it. |
| Failed or premature approach | What was tried too early, removed too soon, or would mask the defect. |
| Permanent rule | A reusable rule stated without board-specific assumptions. |
| Evidence level | The highest level actually achieved and its explicit limit. |

## Closeout checklist

Before closing a driver bring-up phase, record the board identity from
`fbuild port scan`, wiring, command, merge SHA, decisive RPC output, resource
snapshot, and same-session recovery control. Audit every stall or silence for
`fl::print`, `Serial.print*`, `FL_PRINT`, `FL_WARN`, `AR_FL_WARN`, equivalent
macros, and JSON-RPC chatter before changing a timeout. Preserve a proven path
until a replacement has physical wire evidence at an equal or higher level.

The RP2040 and ESP32-WROOM incident narratives follow in the next phases.
