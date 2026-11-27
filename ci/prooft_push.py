"""`./push`: hand off to `prooft --push` (forge-ci/ci.yml GATE-014).

prooft runs the lanes in ci-attestations.yml (lint first, `--test` adds the
host unit tests), stamps HEAD with Ci-Attestation trailers, pushes with
`--force-with-lease` and opens or reuses the PR. This module only checks that
a new enough prooft is on PATH and then replaces itself with it.
"""

import os
import platform
import re
import shutil
import sys

from running_process import RunningProcess


MIN_VERSION = (0, 3, 2)
# 0.3.2 is the first runner release that pushes the attested SHA (not whatever
# HEAD became) and opens the PR in the pushed remote. Pinned in the install hint.
PROOFT_SOURCE = (
    "git+https://github.com/forge-ci/prooft@3445861a03acde3a338c840d582b14c8b0fd22d9"
)
INSTALL_HINT = f"install it with: uv tool install --force {PROOFT_SOURCE}"

_VERSION = re.compile(r"(\d+)\.(\d+)\.(\d+)")


def parse_version(text: str) -> tuple[int, int, int] | None:
    """The first `X.Y.Z` in `prooft --version` output, or None."""
    match = _VERSION.search(text)
    if match is None:
        return None
    return (int(match.group(1)), int(match.group(2)), int(match.group(3)))


def version_problem(version_text: str) -> str | None:
    """Why this `prooft --version` output is not usable, or None when it is."""
    version = parse_version(version_text)
    if version is None:
        return f"could not read a version from `prooft --version`: {version_text.strip()!r}"
    if version < MIN_VERSION:
        need = ".".join(str(part) for part in MIN_VERSION)
        have = ".".join(str(part) for part in version)
        return f"prooft {have} is too old; ./push needs prooft >= {need}"
    return None


def host_problem(system: str, machine: str, libc: str) -> str | None:
    """Why this host may not stamp the linux-x64 gates, or None when it may.

    prooft stamps every gate of a lane without looking at the host, so a run on
    macOS or Windows would record a false `*/linux-x64/*` proof.
    """
    if (system, machine, libc) == ("Linux", "x86_64", "glibc"):
        return None
    return (
        "the ci-attestations.yml gates are linux-x64; "
        f"this host is {system}/{machine}/{libc or 'no glibc'}"
    )


def main(argv: list[str]) -> int:
    host = host_problem(platform.system(), platform.machine(), platform.libc_ver()[0])
    if host is not None:
        print(f"./push: {host}", file=sys.stderr)
        return 2
    prooft = shutil.which("prooft")
    if prooft is None:
        print(f"./push: prooft is not on PATH; {INSTALL_HINT}", file=sys.stderr)
        return 2
    result = RunningProcess.run(
        [prooft, "--version"],
        check=False,
        timeout=60,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    problem = (
        f"`prooft --version` exited {result.returncode}"
        if result.returncode != 0
        else version_problem(result.stdout or "")
    )
    if problem is not None:
        print(f"./push: {problem}; {INSTALL_HINT}", file=sys.stderr)
        return 2
    os.execv(prooft, [prooft, "--push", *argv])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
