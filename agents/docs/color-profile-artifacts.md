# Colour-profile artifacts: checking, ingesting, and why findings are not errors

The emitter profiles FastLED compiles in are generated from artifacts in
[FastLED/datasheets](https://github.com/FastLED/datasheets). This doc covers
the three commands around that, and one policy exemption that matters.

## The policy exemption, first

**Freshness findings are status, not errors.** They are explicitly exempt from
the repo's "fix ALL encountered errors immediately" rule.

A finding means upstream has something this repo does not mirror yet. That is
information for a maintainer, not a defect to repair. Acting on one means
re-mirroring artifacts and regenerating a header, which is a reviewed change
to what the firmware ships — a decision, not a cleanup.

So: **do not ingest because a checker mentioned it.** Ingest on an explicit
maintainer request, and always as a PR.

## The three commands

### `check` — read-only, safe any time

```bash
uv run python -m ci.generate_profile_header --mode check
```

Verifies the checked-in generated header still matches what the generator
produces from the mirrored artifacts. Touches nothing, no network. Exits
non-zero on drift, which is a real error: it means the header and the
artifacts disagree.

### `write` — regenerates, for a maintainer PR

```bash
uv run python -m ci.generate_profile_header --mode write
```

Rewrites `tests/fl/gfx/colorimetric_response_profiles.hpp`. Review the diff.
This is the ingest half of C9.1 and never runs automatically.

### Freshness — network, informational, always exits 0

```bash
uv run python -m ci.color_profile_freshness           # human report
uv run python -m ci.color_profile_freshness --json    # machine readable
```

Compares the mirrored artifacts against upstream. It **always exits 0**,
including when it cannot reach the network — in which case it says
`could not check`, rather than reporting "up to date" for a comparison it
never made.

Deliberately outside `bash lint`: a check that can fail because DNS blinked
has no business gating a commit.

A weekly workflow (`.github/workflows/color_profile_freshness.yml`) runs it
and keeps **one** standing issue current. One issue, edited in place — a new
issue per run turns a weekly informational check into a notification stream,
and the first thing anyone does with that is mute it.

## What the generator will refuse, and why that is correct

Most artifacts cannot produce a profile, and the generator says so instead of
inventing one. The registry deliberately holds catalog records whose optical
data is incomplete — a datasheet publishing dominant wavelength and
millicandela but no chromaticity is one of those.

Both parts mirrored today are exactly that case. The refusal is rendered into
the generated header as a named comment with its reasons, so a part that
produced nothing does not read as a part nobody catalogued.

Do not "fix" a refusal by supplying plausible numbers. Converting a dominant
wavelength to a spectral-locus chromaticity produces values that look like a
profile and are not one, and the schema forbids it:

> only a profile having usable finite xy/Y channel data can generate an
> `EmitterProfile` […] it does not turn missing PDF information into a
> fallback profile

The fix for a refusal is upstream measurement, which is P10's gate.

## Related

- `ci/color_profile_generator.py` — the artifact → C++ bridge and its gate
- `ci/color_profile_freshness.py` — the informational checker
- FastLED#4037 (P3), FastLED#4035 (P1, the artifacts themselves)
