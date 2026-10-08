# Legacy controller ownership correction (#4784)

The maintainer requires ChannelOptions and new emitter/source profile bindings
to belong exclusively to Channels. Legacy addLeds remains the compact API.

- [x] Trace the original LEDSettings -> ChannelOptions replacement and profile API.
- [x] Reproduce matched dashboard Uno Blink: 3914 bytes flash, 266 bytes RAM.
- [x] Record RED legacy layout regressions on host and real AVR compiler.
- [x] Restore compact legacy settings and move options/profile ownership to Channel.
- [x] Preserve base/global setters, mixed managed power, and legacy white/gamma output.
- [x] Verify focused sanitizer tests, full native tests, lint, and independent review.
- [x] Measure Uno Blink ELF with identical sketch, flags, and toolchain (repeat after final commit).
- [ ] Push a PR, complete required hosted validation, merge, and verify merged sizes.

Scope excludes build/unity restructuring and unrelated scratch-array/timer work.
Traditional color-correction/temperature enums remain supported. The newer
profile-specific legacy factory overload is intentionally removed; profiles
use Channel::create<Profile>() or FastLED.add<Profile>(ChannelConfig) on supported
non-AVR platforms.

## Baseline evidence

Base source: 0b3edcd800 (same AVR implementation as dashboard ce225edab6).
Matched benchmark/Blink.ino and benchmark/config/uno.ini from FastLED/dashboard,
including its Arduino entry-point stub and delay(0). fbuild 2.5.37, AVR GCC
7.3.0-atmel3.6.1-arduino7, Arduino AVR core 5.4.0. The baseline reproduces the
dashboard's fbuild 2.5.38 physical totals exactly.

Host regression fails with sizeof(CLEDController) 168 versus ChannelOptions 128.
Real AVR regression fails the <=32-byte CLEDController layout ceiling; the
baseline concrete NEOPIXEL controller occupies 87 bytes. Artifacts are retained
under .build/issue4784 with independent baseline and candidate source projects.

## Candidate measurements

The matched dashboard sketch builds at 3,762 bytes flash and 168 bytes physical
static RAM, saving 152 bytes flash and 98 bytes RAM from the reproduced baseline.
The concrete NEOPIXEL controller is 27 bytes versus 87 bytes before. The 32-byte
controller scratch array is unchanged and outside this issue. No new profile,
pipeline, or soft-float symbols match the inspection filter in the candidate.

Repository Blink has a different entry point: 3,770 bytes image flash and
194 bytes attributed RAM (physical RAM 168). Its budget is tightened from
3,922/260 to 3,770/194, using the same metrics as the existing budget checker.
Symbol attribution includes aliased vtables and must not be quoted as physical
RAM usage. Both ELF/symbol reports and build logs are retained under
.build/issue4784; repository Blink reports are in .build/symbols/uno.

Independent review found and corrected standalone managed power estimation,
UCS7604 gamma ownership, per-frame
pipeline rebuilding on base dithering setters, and an unused profile setter.
The sanitizer regression holds a shared pipeline over profile removal and
checks that dithering preserves its identity.

The generic `asChannel()` integration query replaces profile-specific base
hooks and works for both standalone and registered Channels. It retains no
profile payload and is absent on AVR, along with settings notifications.
An AVR production-header assertion enforces the <=32-byte base layout.

Dashboard source revision: 954622d26efe511ca1234431c7dd0c204fa84173.
Baseline ELF SHA-256: 77612cc96afff5fc20b60bbdecc7383f45a6acd94408e4dd28c946bcf80767fb.
Candidate ELF SHA-256: c3447048ec669ecefbadbe0c8fba8af5542b074f9ee2b528639ec551499920e6.

Review: one primary reviewer checked C++ source/tests, documentation and size
budget configuration against repository rules; final verdict clean. Focused
standalone managed power sanitizer regression passes after the identity fix.

Final compatibility checks explicitly run both non-AVR TINY targets through
`bash test --unit color_profile_tiny_runtime --debug` and
`bash test --unit color_profile_tiny_layout --debug`. Applying a legacy
configuration must clear a static profile: the added runtime probe fails at
exit 9 before correction and passes afterwards. AVR also excludes the Channel
static identity flag/virtual and rejects direct specialized construction.

Full sanitizer pass: 318 unit targets and 95 examples. The full Python suite
passes; repository lint and focused power/TINY probes pass. ESP32-S3 CI
run 37742672330 measures image350151 /attributedRAM38323, saving232 bytes
flash with RAM unchanged; its existing ratchet and budget are tightened to
claim this observed saving.

ATmega8 CI reports existing example capacity failures. An isolated original
0b3edcd800 RGBW build overflows 8 KB by874 bytes, versus838 in PR CI. Clean
RGBWColorimetric builds measure7322/656 before and7264/645 after; the CI
sequence discrepancy is checked with a base-commit dispatch, run37745117666.
This issue does not change unrelated examples, drivers, or board selection.
