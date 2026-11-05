# Generic engine worker defaults

User request: generic `FL_` macros for worker core and priority which each
peripheral may honor or ignore. Base: merged #5023,
`074d9dbf8857d57d5c9cc246cb3e90a9bcb0c0b4`.

- [x] Inspect current scheduling, defaults, validation and shared-engine locks.
- [x] Add RED coverage for macro defaults and explicit Channel precedence.
- [x] Add `FL_ENGINE_WORKER_CORE` and `FL_ENGINE_WORKER_PRIORITY` as opt-in
  compile-time defaults; use them in the LCD clockless worker's real path.
- [x] Keep WorkerConfig defaults and unsupported peripherals unchanged; retain
  shared-policy conflict handling and reject invalid supported-driver values.
- [x] Document values, precedence, build-wide definitions and driver support.
- [x] Pass focused variants, full native validation, lint, SDK compilation and
  independent code review.
- [ ] Push and merge a reviewed PR after exact-head platform/native CI.

Precedence: explicit Channel settings before worker creation, then compile
defaults, then the peripheral's existing behavior. Compile defaults alone do
not lock out the first explicit request. Once an explicit request or worker
creation establishes shared scheduling, existing conflict rules still apply.
Legacy templates remain free of worker settings; generic compile defaults
may affect their shared engine when that peripheral opts in.

The user confirmed portable named priorities. Comments define High as at or
above networking, biased above; Medium below networking; Low generally not
recommended. Unset macros preserve existing affinity/priority. Macros must be supplied to all
translation units, not just defined in a sketch that links separately compiled
drivers. No new workflow files or local release/tagging are part of this task.

RED evidence: the isolated real production library with Core1/Medium flags
failed the worker core==1 assertion on the original implementation (6.59s,
debug mode). WorkerConfig and unsupported-driver default checks passed first.
The fixture uses the public no-tracking shared_ptr factory for the static
peripheral mock. Evidence: ignored `.build/evidence/engine-worker-macros/`.

GREEN: actual Core1/Medium runtime variant passes (73.99s). Default full debug
validation passes 330 unit groups and 95 examples (463.56s). All 17 focused
Python cases pass (7.67s), including six C++11 invalid-policy compile checks
and the real unit-only runtime wrapper gate. An initial overlapping gate was
cancelled and rerun serially; no cancelled result counts as validation.
S3 AutoResearch passes with global Core1/Medium definitions (3m46s) and with
both macros unset (3m17s). Full lint and independent 12-file review are clean.

Hosted publication remains pending; the PR's exact-head CI, review and merge
record prove completion beyond this pre-publication snapshot.

Integration: master advanced to `94dab9c6b720ea277ec9177c93129c7dc646c5c3`
(#5021, generic ESP8266 SPI). The only merge conflict was the task tracker;
both task sections were retained. Production edits are disjoint. The combined
tree passes 331 unit groups and 95 examples (270.10s), all 17 focused Python
cases including the real macro gate (9.14s), and S3 Core1/Medium compilation
(3m34s). Independent integration review confirms the feature code is unchanged.
