# FastLED Release howto

*Pushing a fastled release, the short version, last updated May 2024*

## How to release (automated)

1. Make sure `release_notes.md` opens with a `FastLED X.Y.Z (Next Release)` section holding the notes.
2. `bash release prepare --pr` (add `minor` or `major` for a bigger step; default is `patch`). It bumps every version string one step past the newest tag, makes the notes heading final, and opens a `Rev X.Y.Z` PR.
3. Review and merge that PR. **Merging it is the release**: the `release` workflow tags the merge commit, creates the GitHub release from the notes, and rebuilds the docs. The Arduino index picks up the tag; the package registry's crawler picks up the new version from master.
4. `bash release status` until every row shows the new version, then announce it on the subreddit.

The manual steps further down are what the automation replaces; they remain as a fallback.

## Release helper script

`bash release` (see `ci/release.py`) uploads nothing itself:

  * `bash release check` -- the version strings agree and match the newest tag. `--releasing` is the release-PR state: exactly one step ahead, notes heading final. `bash lint` runs this check.
  * `bash release prepare [patch|minor|major] [--pr]` -- write the next version everywhere; with `--pr`, branch, commit, push and open the PR.
  * `bash release notes X.Y.Z` -- print that version's section of `release_notes.md`.
  * `bash release status` -- what the newest tag, GitHub Releases, the Arduino index and the package registry each have.

**Why master must not run ahead of the newest tag:** the package registry's crawler publishes whatever version `library.json` / `library.properties` show on the default branch. No tag or GitHub release triggers it; a tag named after the version only decides which tree it packages. So the version moves only in the release PR, and the workflow tags its merge commit immediately.

## Example

https://github.com/FastLED/FastLED/commit/4444758ffaf853ba4f8deb973532548c9c1ee231

## How to

Edit these files to update the version number
  * library.json 
  * library.properties 
  * src/FastLED.h 
  * docs/Doxyfile
  * RELEASE.md
    * This file: update instructions with the current release.


Edit this file with release notes and version number.
  * release_notes.md

Release notes should list highlight changes (not necessarily all minor bug fixes) and thank people for their help. 

Git commands to commit and tag release'
```bash
$ git commit -am "Rev 3.10.3"
$ git tag 3.10.3 master
$ git push
$ git push origin 3.10.3
```

Then use the GitHub UI to make a new “Release”:

https://github.com/FastLED/FastLED/releases/new

Announce new version on subreddit, highlighting major changes and thanking people for helping.

That's it.

## Detailed Step-by-Step Instructions

### 1. Prepare Release Notes
Before starting the version update, ensure `release_notes.md` has an entry for the new version at the top of the file with:
- Version number as a header (e.g., `FastLED 3.10.3`)
- Major changes and fixes since the last release
- Each change should be well-documented with bullet points
- Include references to GitHub issues where applicable (e.g., `#2067`)
- Credit contributors when appropriate

### 2. Update Version Numbers
Update the version number in the following files:

#### library.json
- Find the `"version":` field (around line 41)
- Update to the new version (e.g., `"version": "3.10.3",`)

#### library.properties
- Find the `version=` line (line 2)
- Update to the new version (e.g., `version=3.10.3`)

#### src/FastLED.h
Three places need updating:
- Line ~20: `#define FASTLED_VERSION 3010003` (format: XYYYZZZZ where X=major, YYY=minor, ZZZ=patch)
- Line ~24: `#pragma message "FastLED version 3.010.003"`
- Line ~26: `#warning FastLED version 3.010.003  (Not really a warning, just telling you here.)`

#### docs/Doxyfile
- Find `PROJECT_NUMBER` (around line 51)
- Update to the new version (e.g., `PROJECT_NUMBER = 3.10.3`)

#### RELEASE.md
- Update the git commands in the example to show the new version number

### 3. Commit Changes
```bash
$ git add -A
$ git commit -m "Rev 3.10.3"
```

### 4. Create Git Tag
```bash
$ git tag 3.10.3 master
```

At this point halt with the message to tell the user to do the following steps:

### 5. Push to Repository
```bash
$ git push
$ git push origin 3.10.3
```

### 6. Create GitHub Release
1. Go to https://github.com/FastLED/FastLED/releases/new
2. Select the tag you just created
3. Set release title to the version (e.g., "3.10.3")
4. Copy the release notes from `release_notes.md` for this version
5. Publish the release

### 7. Announce the Release
Post on the FastLED subreddit (r/FastLED) with:
- Version number
- Highlights of major changes
- Thanks to contributors
- Link to the GitHub release 
