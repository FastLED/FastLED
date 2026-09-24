# CI modes (fractional CI pilot)

This is the first slice of [FastLED #4543](https://github.com/FastLED/FastLED/issues/4543).
Ordinary PR and `master` events skip the reusable fbuild board and size jobs,
plus QEMU, WASM, AVR8JS, and ESP32-S3 bloat checks.
They run a named Linux/Windows native unit smoke inventory from
`ci/native_ci.py`, the small Python color/CI guard smoke inventory on Linux,
and Linux `CompileTests` examples. The Windows example suite and full native
unit/Python breadth move to `ci-full`; nothing is deleted. Run
`bash ci-native cpp` or `bash ci-native py` to inspect the exact smoke list.
The three C++ smoke targets share one Meson build and its `fastled.so`, while
the standalone Linux example lane still builds separately; further sharing
requires measured benefit and memory safety.
An internal PR labeled `ci-full` runs all 109 jobs in 90 selected workflows.
It also re-runs complete native Linux/Windows/hosted Intel+Apple Silicon
macOS unit suites, the full Linux Python suite, and Linux/Windows/macOS
example suites on the same PR head SHA. `labeled`/`unlabeled` retrigger these
native wrappers without requiring another commit.
`workflow_dispatch` still runs an individual board workflow when explicitly
requested. Selected board/test and native unit/example workflows use the PR
head commit for checkout, and labeled
and unlabeled events recompute the selection without a new commit.

Run `bash ci-labels list --json` to see every exact `ci-platform:<board>`,
approved prefix label, and `ci-test:` family. `bash ci-labels expand 'ci-platform:esp*'` shows its
concrete build, size, QEMU, and bloat jobs; `bash ci-labels expand 'ci-platform:teensy41'` includes the
Teensy 4.1 size gate. `ci-test:qemu*`, `ci-test:wasm`, `ci-test:avr8js`, and
`ci-test:bloat` request independent longer-running suites. Multiple labels are additive. Unknown `ci-platform:` or
`ci-test:` names fail the CI board selection check. To update generated gates
after changing a board wrapper, run `bash ci-labels sync` and commit the
result. `bash ci-labels check` detects drift. Maintainers can run
`bash ci-labels labels-check` and `bash ci-labels labels-sync` to reconcile
the emitted catalog with GitHub labels. Existing `platform:` labels are
metadata and never select jobs. Fork PRs cannot execute board or selection
jobs.

One historical ordinary source-change PR used 128.89 runner-minutes across
20 PR workflows
([head `cd48ec4`](https://github.com/FastLED/FastLED/pull/4418)); the merged
`master` push used 903.95 runner-minutes across 99 workflows
([SHA `df106df`](https://github.com/FastLED/FastLED/commit/df106dfe6a2124c03143ef611c7d1174ea70f06c)).
Both figures sum positive Actions job `completed_at - started_at` for all
executed jobs with the event and SHA. They are historical routine-event costs,
not the denominator for the <10%-of-full acceptance metric. On one exact SHA,
an unlabeled Phase 1 PR event used 30.57 runner-minutes while its labeled
full sweep used 1011.14 runner-minutes (3.02%); that full sweep had one
ESP32-S3 timeout after its compilation succeeded, so it was not a full pass.
The selected native smoke subset further reduces routine cost, but remains a
measured candidate, not acceptance. The final exact-SHA ordinary PR and
`master` events must each use <10% of their matched *full* event's raw
runner-minutes before the pilot can merge. The full event
must prove every selected cell and every hosted Mac variant on that same SHA.
`tests/meson.build` currently names three serial tests, but only
`channel_driver_uart` is live; the custom streaming runner does not consume
Meson's `is_parallel` metadata. Keep smoke execution serial until selective
parallelism has RED→GREEN exclusion tests and peak-RSS/OOM evidence.

`release.yml` is manual only. After merging a release version bump, record the
exact `master` SHA and run `GH_TOKEN=... GITHUB_REPOSITORY=FastLED/FastLED uv run
python ci/release_gate.py dispatch-full --sha <sha>` from a trusted checkout.
The dispatcher requires that SHA to remain the current `master` head and
dispatches every workflow in the board/test selector catalog plus complete
Linux, Windows, and hosted Intel and Apple Silicon macOS unit/example suites.
Wait for these runs to finish. Then manually run `release.yml` on `master`
with `candidate_sha=<sha>` and `dry_run=true`. Its evidence gate reads each
workflow's completed dispatch run, requires the same SHA and successful jobs,
and requires both hosted macOS variants. After the dry run passes, repeat with
`dry_run=false` to create the tag and release at that SHA. The real run
re-fetches `origin/master` immediately before the tag step and refuses a
candidate that has since been superseded. The check narrows, but cannot
eliminate, the network race between fetching and GitHub accepting the tag.
An ordinary version push, a green fractional run, or an individual board
dispatch cannot tag.

The package registry crawler may read a new default-branch version before
the tag exists. The release gate prevents an untested GitHub tag/release, but
cannot prevent that external crawler from reading the merged version while
the exact-SHA full sweep runs. Plan release timing with that constraint.
