# ESP32 flash optimization restart checkpoint

## State

Paused at the user's request on 2026-10-07 for a computer restart.
Branch: `checkpoint/esp32-flash-4707-unity`.
Parent: `d67c812e6c`. Existing PR: https://github.com/FastLED/FastLED/pull/4736
Original issue: https://github.com/FastLED/FastLED/issues/4707 (keep open).
No merge or release is authorized. This checkpoint is experimental.

## Goal and maintainer direction

Reduce flash/RAM toward 3.10.3 as far as viable while preserving the RMT rewrite, public APIs and supported features. Examples/public gist suffice; private sketch is unnecessary. Narrow the 356-file PR and restore larger unity groups where measurements allow. Investigate constant initialization and valid noexcept/section contracts before further splitting; no universal annotation makes referenced code removable.

## Saved working optimization

1. Keep all GPIO/ADC/PWM implementations in one optional canonical unit to preserve single SDK state ownership. Public power-indicator setter installs a private callback; core calculations no longer root generic GPIO. Explicit ISR has its own canonical owner.
2. Return corkscrew and geometric noise to existing `fl.gfx` unity owner, deleting their two separate canonical entries. Include order: corkscrew, detail, noise.
3. Preserve existing source and API bodies. The setter keeps its published exception signature; migrated lint baselines record existing debt.

## Measurements (allocated image flash; default Blink)

| Profile | Before GPIO | GPIO + consolidated graphics | Static DRAM |
|---|---:|---:|---:|
| esp32dev_idf44 | 274157 | 273173 | 17496 |
| esp32dev | 289923 | 289927 | 25036 |
| esp32s3 | 353591 | 353571 | 23416 |

Graphics consolidation alone changes none of these three image totals. GPIO static DRAM is unchanged. Modern GPIO flash increases 4 bytes; do not claim a universal reduction.

Before GPIO, matching old Serial Blink on legacy was 289433 versus 3.10.3 262757: gap26676 flash/248 DRAM. Public gist modern was296311 versus274921: gap21390 flash. These controls have NOT been remeasured for this checkpoint. Do not compare new minimal Blink with old Serial Blink as a library saving.

## Validation

- GPIO full native:323/323 tests,95/95 examples passed,344.87s.
- GPIO power dispatch, pin/PWM and ISR sanitizer tests passed.
- Explicit GPIO/ADC/PWM/power-indicator/ISR SDK consumer compiled on legacy+modern; no hardware-runtime claim.
- Graphics consolidation C++ lint passed after fixing lexical include order.
- Consolidated default Blink compiled on legacy,modern,S3; bloat totals above verified.
- Consolidation full native run was deliberately interrupted for restart; NOT a test failure or a completed pass. Rerun before publication as validated production code.
- Independent read-only reviews checked GPIO ownership/ODR and consolidated graphics helper/guard interactions.
- Published38bdb4a0c8 hosted focused size run37675361815 succeeded (353607 default S3); checkpoint has no hosted exact-SHA evidence.

## Resume steps

1. Fetch, switch to checkpoint branch, inspect clean status and this file. Read CLAUDE.md and applicable agents docs.
2. `bash test --cpp`; fix failures with targeted debug rerun. `bash lint --cpp`.
3. Rebuild with `bash compile <board> --examples Blink` and `bash bloat <board> --no-summary`; preserve reports before any next build. Profiles above. Physical DRAM is sum of map .dram0.data/.dram0.bss, not report total_ram (includes IRAM).
4. Remeasure same Serial Blink and public gist controls. Scratch sketches were in /tmp and may not survive reboot; recreate from original controlled source/public gist, preserving identical sketch, pin12 and flags. Gist URL:https://gist.github.com/chemdoc77/52ded62fc801247cc648b1eb84f9d2ca
5. Consolidate additional optional graphics drawing/effects/paths/blur into gfx only after auditing macro/helper interactions and measuring real users. Bitset/IEEE formatting may share optional string_operations owner. Keep measured core boundaries until actual evidence permits restoration.
6. Separate generic native-runner repairs into their own PR: ci/meson/{runner,streaming,streaming_runner,test_execution}.py;ci/util/test_runner.py;test.py;four associated tests (compile_deadline,example_runner_exec_bit,forced_native_execution,streaming_runner_artifact_validation);root meson.build;tests/meson.build alias/probe hunks. KEEP seven-line real RMT allocator test-source integration with size PR. Removing runner repairs without replacement would lose truthful cached-suite validation.
7. Generic AST deduplication repair can be separate:ci/tools/check_ast_combined.py,check_noexcept.py,ci/tests/test_ast_shared_declarations.py.
8. Update issue/PR with measured results; claim hosted ratchet only after downloading exact-SHA evidence. Leave ci-full off unless required/authorized for next phase; it was removed externally.

## Outstanding external dependency

Production Windows is blocked by published reld0.2.3 passing an ELF-only linker flag to MinGW. External reld PRs220/221 fix this and prepare0.2.4, but permission to merge/release is still unanswered. Do not merge/tag/release based on this checkpoint. Prior diagnostic Windows passes are for older source, not checkpoint proof.

## Local evidence location

.build/size-4707/{gpio-closure,unity-consolidation}-*.json and /tmp/fastled-4707-*.log. These are local only; important measurements/status are reproduced here for reboot safety. Reproduce all new gates after resuming.
