# Channels-only worker scheduling (#5001 follow-up)

The requested final API keeps both LED worker core affinity and priority in
ChannelConfig worker settings. #5022 removed legacy core selection; this
follow-up removes legacy priority selection too.

- [x] Prove legacy priority acceptance regressions fail before implementation.
- [x] Remove legacy priority templates and scheduling plumbing; preserve bus
  selection, SPI host/index overloads, custom encoders and default behavior.
- [x] Verify ChannelConfig retains core and priority control on the real driver.
- [x] Update migration guidance and mark earlier API descriptions historical.
- [x] Pass focused/full native debug validation, relevant Python tests, lint,
  and canonical ESP32-S3 AutoResearch compilation.
- [x] Complete independent repository code review.
- [ ] Push a feature PR with full/native CI labels applied at creation, resolve
  review findings, observe full exact-head CI, and merge normally.

Production consumers are legacy addLeds calls and the ChannelConfig path used
by AutoResearch. Legacy calls must not configure shared worker scheduling.
General task scheduling and platform defaults remain outside this API change.
No memory savings are claimed without a matched measurement. No release,
version tag, workflow addition or hardware deployment is part of this task.

RED evidence: `bash test tests/fl/channels/slim_bridge_controller.cpp --debug`
fails exactly the clockless/SPI priority-acceptance assertions on base
`e5016e7c09f082bb2e9f4ed332532a99b43e9ea7`; core-rejection assertions and
23 other cases pass. Log: ignored
`.build/evidence/issue-5001-channel-scheduling/red-legacy-priority.log`.

The same focused debug test passes after implementation (155.04 seconds,
including the changed-header rebuild). All 217 AutoResearch phase tests pass
(20.61 seconds); canonical ESP32-S3 AutoResearch compilation passes (2m04s).
The existing ChannelConfig forwarding tests and actual LCD driver policy
tests remain, with only the removed legacy WorkerPolicy calls deleted.
Full lint passes, and hashes confirm lint did not modify any of the 12 changed
C++ files. Independent repository review of all 20 changed/new files is clean.
Full native debug validation passes all 330 unit groups and 95 examples
(394.36 seconds including the changed-header rebuild). This includes the
ChannelConfig core/priority forwarding and real LCD driver's named policy,
conflict and affinity checks. Evidence logs share the directory above.
No fresh flash or RAM savings measurement is claimed.

Publication completion is proved by the follow-up PR's current review,
exact-head full CI coverage and merge record, rather than this pre-publication
snapshot.
