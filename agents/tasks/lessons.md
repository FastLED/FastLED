# Lessons Learned

<!-- Add lessons from corrections and discoveries here -->

- Inspect the actual WASM compiler manifest before assuming an npm dependency: it
  currently pins the `@fastled/gfx` GitHub Release tarball by exact URL.
