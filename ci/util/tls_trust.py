"""Point `SSL_CERT_FILE` at a CA bundle that exists on this machine.

Every `clang-tool-chain-*` invocation version-checks its manifest over HTTPS.
On distributions whose Python is built expecting `/etc/ssl/cert.pem` -- NixOS
among them -- that file does not exist, and the `capath` fallback
(`/etc/ssl/certs`) carries no `c_rehash` symlinks, so *every* such request dies
with `CERTIFICATE_VERIFY_FAILED`.

The failure does not look like a network problem. The wrapper prints its SSL
traceback where the build generator expects a version string, so configuration
aborts with the thoroughly misleading::

    ERROR: Unknown linker(s): [['.../clang-tool-chain-ar']]

which sends you looking at the linker, or at PATH, or at the toolchain
install -- none of which are wrong.

This lives in `ci.util` rather than in the build-setup module because the
wrappers are invoked from at least nine entry points under `ci/` (WASM builds,
IWYU, the AST checker, the compiler probe, the DLL path helper). Fixing it in
one of them fixes exactly one. `ci/__init__.py` calls this on package import
and all nine import `ci`, so the environment is right before any of them makes
its first HTTPS call.
"""

from __future__ import annotations

import os
from pathlib import Path


def ensure_tls_trust_store() -> None:
    """Set `SSL_CERT_FILE` if it is unset and a real bundle can be found.

    Set only when the caller has not already chosen one, and only to a path
    that is really there, so this can neither override a deliberate
    configuration nor invent a broken one.

    An *empty* `SSL_CERT_FILE` counts as not chosen, deliberately. OpenSSL
    cannot load `""` as a trust store, so honouring it would leave the caller
    with the exact failure this function exists to prevent and no benefit to
    anyone; and an empty value is nearly always an accidental expansion of an
    unset variable rather than a considered setting. Unsetting the variable,
    not emptying it, is how you ask for OpenSSL's own defaults -- and that
    path is untouched here.

    Finding no candidate is not an error: Windows and macOS have neither file
    and Python's default trust store works there. Any genuine TLS failure is
    still reported by the caller, with its full traceback.
    """
    if os.environ.get("SSL_CERT_FILE"):
        return

    candidates = [Path("/etc/ssl/certs/ca-certificates.crt")]
    try:
        import certifi  # noqa: PLC0415 - optional, and only needed on this path

        candidates.append(Path(certifi.where()))
    except KeyboardInterrupt:
        import _thread  # noqa: PLC0415

        _thread.interrupt_main()
        raise
    except Exception:
        # certifi is optional; the system bundle above is the common case.
        pass

    for candidate in candidates:
        try:
            if candidate.is_file():
                os.environ["SSL_CERT_FILE"] = str(candidate)
                return
        except OSError:
            continue
