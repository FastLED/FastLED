# FastLED Release howto

*Pushing a fastled release, the short version, last updated May 2024*

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

That’s it. 
