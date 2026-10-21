/// @file fl.system.sd+.cpp
/// @brief Tombstone: intentionally empty. Do not delete.
///
/// FastLED 3.10.4-3.10.5 shipped the SD unity unit under this name. 3.10.6
/// renamed it to `fl.fs.sd+.cpp`. A library unpacked over an older copy
/// (Arduino IDE "Add .ZIP Library", a manual copy) keeps the stale file,
/// and Arduino compiles every `.cpp` under `src/`, so both units defined
/// `fl::FileSystem::beginSd(int)` and the link failed (FastLED #4704).
/// Shipping this empty file under the old name overwrites the stale copy.
/// `ci/check_released_sources.py` enforces the pattern at release time.
