# CI modes (board selection pilot)

This is the first slice of [FastLED #4543](https://github.com/FastLED/FastLED/issues/4543).
Ordinary PR and `master` events skip the reusable fbuild board and size jobs,
plus QEMU, WASM, AVR8JS, and ESP32-S3 bloat checks.
An internal PR labeled `ci-full` runs all 109 jobs in 90 selected workflows.
`workflow_dispatch` still runs an individual board workflow when explicitly
requested. Board workflows use the PR head commit for checkout, and labeled
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

This slice does not yet satisfy the strict whole-event compute target. One
matched source change used 128.89 runner-minutes across 20 PR workflows
([head `cd48ec4`](https://github.com/FastLED/FastLED/pull/4418)); the merged
`master` push used 903.95 runner-minutes across 99 workflows
([SHA `df106df`](https://github.com/FastLED/FastLED/commit/df106dfe6a2124c03143ef611c7d1174ea70f06c)).
Both figures sum Actions job `completed_at - started_at` for all runs with
the event and SHA. The existing Linux unit and example jobs alone used
24.95 PR runner-minutes, above the 12.889-minute strict 10% PR threshold.
The next slice must share the native build and run unit and example suites
in parallel, then measure complete ordinary PR and `master` events again.

The existing `release.yml` still tags automatically on a `master` version
change, before exact-commit full CI can pass. `ci/release.py` documents that
the package registry crawler can publish a default-branch version without a tag.
Until the release control issue resolves that ordering, do not treat a
version bump, an individual board dispatch, or a green minimal run as
release approval. Release candidates need all platforms and boards, both
hosted Intel and Apple Silicon macOS, and a complete full gate on the exact
commit before any tag is created.
