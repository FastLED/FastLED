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

## RP2040 / Pico postmortem

The Pico history is a transport-first lesson: a board may be healthy while a
deploy changes its application CDC port, and a successful serial attach does
not prove that an RPC handler consumed a command. Board identity belongs in
[FastLED/boards](https://github.com/FastLED/boards); record it with `fbuild
port scan` at run time instead of freezing a VID/PID table in this guide.

| Incident | Sources | False lead and discriminator | Root cause and working fix | Permanent rule / evidence |
| --- | --- | --- | --- | --- |
| Port reacquisition | [#3633](https://github.com/FastLED/FastLED/issues/3633), [`f7df508ddc`](https://github.com/FastLED/FastLED/commit/f7df508ddc), [`8b6b5958db`](https://github.com/FastLED/FastLED/commit/8b6b5958db), [`d4d93a4607`](https://github.com/FastLED/FastLED/commit/d4d93a4607) | An empty deploy-port marker was interpreted as a dead board. A fresh fbuild scan plus a post-deploy application-port reacquisition distinguished expected CDC replacement from failure. | Treat the deploy result as the transport authority and reconnect through the fbuild-backed adapter. | Never cache a pre-flash port or infer BOOTSEL failure from a disappearing application CDC port. Evidence: board transport/RPC, not driver proof. |
| RPC contract | [`1ec820b372`](https://github.com/FastLED/FastLED/commit/1ec820b372), [`0534dcc69e`](https://github.com/FastLED/FastLED/commit/0534dcc69e) | A silent command was assumed to be a serial failure. `help`/discovery plus a bounded unknown-method probe separated a missing binding from transport loss. | Publish structured method manifests and keep a negative probe bounded. | Prove discovery, ping, valid request, and invalid request before testing a peripheral. Evidence: RPC only. |
| Watchdog recovery | [`fdd17ded00`](https://github.com/FastLED/FastLED/commit/fdd17ded00), [`9db07346ed`](https://github.com/FastLED/FastLED/commit/9db07346ed), [#3640](https://github.com/FastLED/FastLED/issues/3640) [`9cebcfac68`](https://github.com/FastLED/FastLED/commit/9cebcfac68), and [#3641](https://github.com/FastLED/FastLED/issues/3641) [`24b4cfcb9e`](https://github.com/FastLED/FastLED/commit/24b4cfcb9e) | A missing acknowledgement was mistaken for a failed reset. Flushing the acknowledgement before the intentional hang and then requiring disconnect, re-enumeration, ping, and RPC smoke separated the two. | Make reset recovery an end-to-end transaction, not a reset assertion. | A watchdog claim needs an acknowledged trigger and a post-reset RPC control. Evidence: recovery/transport; it does not prove LED data. |
| PIO loopback closeout | [#3658](https://github.com/FastLED/FastLED/issues/3658), [#3675](https://github.com/FastLED/FastLED/pull/3675), [#3661](https://github.com/FastLED/FastLED/issues/3661), and [#3676](https://github.com/FastLED/FastLED/pull/3676) | CPU edge counts and a GPIO toggle were initially tempting proof of a clockless driver. The discriminator was an independent PIO RX oracle on the physical GPIO11-to-GPIO8 jumper. | Serialize PIO program/state-machine/DMA/capture ownership; make short-frame capture deterministic; feed the existing watchdog between bounded repeated captures, not by lengthening it. | Keep a physical jumper, exact-byte decode, concurrent-resource check, structured exhaustion rejection, and same-session known-good recovery. Evidence: byte exact, resource, and soak only for the recorded PIO/GPIO matrix. |

The Phase 8 merged-main run on the Pico was 100 LEDs, four patterns, and 16
frames per pattern in 7.229 s. The adjacent 101-LED PIO RX capacity request
returned `dma_buffer_size` in 18 ms and the 100-LED control immediately passed
again. The failed/premature alternatives were extending the host timeout,
calling direct flashing/serial tooling, and treating a GPIO pulse count as
byte proof.

### RP2040 quiet-control rule

For a synchronous RPC soak, first audit `fl::print`, `Serial.print*`,
`FL_PRINT`, `FL_WARN`, `AR_FL_WARN`, and response construction. Success-path
edge dumps and progress strings can create heap/transport pressure even when
their output is disabled. Preserve detailed raw-edge snapshots for RX wait,
decode, and comparison failures; do not create them for every successful
frame. Feed the watchdog only between independently bounded operations so a
real capture or TX wedge remains observable.

The ESP32-WROOM incident narrative follows in the next phase.

## Classic ESP32-WROOM postmortem

Classic ESP32 work repeatedly showed that console readiness, scheduler
progress, peripheral ownership, and wire data are separate layers. Identify
the attached board with `fbuild port scan` and use the production record in
[FastLED/boards](https://github.com/FastLED/boards); CP210x reset behavior is
transport evidence, not an LED-driver conclusion.

| Incident | Sources | False lead and discriminator | Root cause and working fix | Permanent rule / evidence |
| --- | --- | --- | --- | --- |
| Ready versus ingest | [#3446](https://github.com/FastLED/FastLED/issues/3446), [#3447](https://github.com/FastLED/FastLED/issues/3447), [#3452](https://github.com/FastLED/FastLED/issues/3452) | A ready banner or CP210x reset transition was treated as proof the command ran. A bounded RPC request/response and scheduler-progress observation separated console readiness from handler ingestion. | Keep transport, command acknowledgement, and peripheral test as separate gates. | A boot/ready line proves boot only; require RPC evidence before interpreting silence as driver behavior. |
| API seams and ownership | [#3458](https://github.com/FastLED/FastLED/issues/3458), [#3461](https://github.com/FastLED/FastLED/issues/3461), [#3465](https://github.com/FastLED/FastLED/issues/3465), [#3467](https://github.com/FastLED/FastLED/issues/3467), [#3469](https://github.com/FastLED/FastLED/issues/3469), [#3470](https://github.com/FastLED/FastLED/issues/3470), [#3471](https://github.com/FastLED/FastLED/issues/3471), [#3473](https://github.com/FastLED/FastLED/issues/3473) | A singleton mock or a static peripheral claim looked sufficient. Deterministic failure injection plus real RX backends exposed lifecycle and shared TX/RX ownership gaps. | Put resource claims, rollback, stop, and release behind explicit seams testable with real backend behavior. | A mock proves a contract path, not a GPIO/peripheral result; pair it with resource-lifecycle tests. |
| Do not remove wire proof | [#3474](https://github.com/FastLED/FastLED/issues/3474), [#3477](https://github.com/FastLED/FastLED/issues/3477), [#3479](https://github.com/FastLED/FastLED/issues/3479), [#3512](https://github.com/FastLED/FastLED/issues/3512), [#3526](https://github.com/FastLED/FastLED/issues/3526) | A new architecture was assumed better because it compiled or looked cleaner. Raw wire and replacement-path evidence exposed that the proven I2S route had been removed prematurely. | Preserve a known-good data path until the replacement has equivalent physical proof and a rollback plan. | Architecture preference never outranks existing attached-board proof. Evidence: design/static until wire proof is repeated. |
| Loopback qualification | [#3568](https://github.com/FastLED/FastLED/issues/3568), [#3569](https://github.com/FastLED/FastLED/issues/3569), [#3570](https://github.com/FastLED/FastLED/issues/3570), [#3571](https://github.com/FastLED/FastLED/issues/3571) | A jumper, an edge counter, or a decoded partial frame was treated as decisive. Mapping/clock/descriptor/stop/heap/stack checks plus raw edges and byte-exact decode separated those claims. | Qualify an independent RX oracle before attributing a result to TX; include stop and memory behavior in the same lifecycle test. | GPIO proof, raw-edge proof, timing proof, and byte-exact proof are cumulative—not interchangeable. |
| UART margin and pressure | [#3572](https://github.com/FastLED/FastLED/issues/3572), [#3573](https://github.com/FastLED/FastLED/issues/3573), [#3588](https://github.com/FastLED/FastLED/issues/3588), [#3589](https://github.com/FastLED/FastLED/issues/3589), [#3597](https://github.com/FastLED/FastLED/issues/3597), [#3598](https://github.com/FastLED/FastLED/issues/3598), [#3599](https://github.com/FastLED/FastLED/issues/3599), [#3600](https://github.com/FastLED/FastLED/issues/3600), [#3603](https://github.com/FastLED/FastLED/issues/3603), [#3604](https://github.com/FastLED/FastLED/issues/3604) | Passing a nominal waveform hid symbol-separation margin, concurrent pressure, and response/telemetry heap cost. Failure injection, streamed/collated diagnostics, and resource pressure distinguished data-path defects from observability cost. | Bound diagnostic output, retain failure evidence, and test OOM/stack/resource recovery after isolated driver success. | A passing single frame is not a resource conclusion. Require a pressure matrix and a same-session known-good control. |

### ESP32 quiet-control rule

Before increasing a timeout, account for every `fl::print`, `Serial.print*`,
`FL_PRINT`, `FL_WARN`, `AR_FL_WARN`, collated edge dump, and JSON-RPC payload.
Diagnostics are part of the memory and transport budget. Keep raw-edge and
byte-mismatch evidence on failures, cap it in normal paths, and state whether
the conclusion came from an attached board, a host test, or static inspection.

The final integration phase links this guide from the agent runbooks and
consolidates the reusable checklists.
