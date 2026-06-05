"""Linters that detect internal PlatformIO build usage (issue #2701).

This package enforces the fbuild-only board-build invariant. Internal
PlatformIO build call sites (``pio run``, ``platformio run``,
``--backend platformio``, ``--platformio``/``--pio`` flag passing, and
generated ``platformio.ini`` board staging) are forbidden in
build/CI code. User-facing references in docs, examples, library
manifests, and issue templates are explicitly allowed.

See ``ci/lint_platformio/check_no_internal_platformio.py`` for the
checker and ``run_all_checkers.py`` for the dispatch entrypoint used by
``bash lint``.
"""
