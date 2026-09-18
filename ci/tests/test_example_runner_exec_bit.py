"""The example path must restore example_runner's executable bit (FastLED#4205).

`restore_executable_bits` already scans `<build>/examples`, but only the unit
test paths (`meson test` streaming, and `run_direct_test`) call it. The
example runner, `bash test --examples X --full`, reached `meson test` without
it, so an `example_runner` that zccache had served from its link cache stayed
non-executable and every example died with an opaque
`PermissionError: [Errno 13]` / "Examples failed (return code 13)".

This drives the real `run_examples` against a real executable rather than
asserting that a particular function gets called. The stub tree is not a
configured Meson build, so `meson test` itself fails; the claim under test is
only that the restore has already happened by then.
"""

import os
import shutil
import stat
from pathlib import Path

import pytest
from typeguard import typechecked

from ci.util.meson_example_runner import run_examples


@typechecked
def _stub_example_runner(build_dir: Path) -> Path:
    """An `examples/example_runner` in the state zccache leaves it: 0644.

    A copy of a real system executable, because `restore_executable_bits`
    only touches files carrying a native-executable magic number.
    """
    real_exe = shutil.which("sh")
    if real_exe is None:
        pytest.skip("no system shell to stand in for the example runner")

    examples = build_dir / "examples"
    examples.mkdir(parents=True)
    runner = examples / "example_runner"
    shutil.copyfile(real_exe, runner)
    runner.chmod(0o644)
    return runner


@pytest.mark.skipif(os.name == "nt", reason="POSIX mode bits")
@typechecked
def test_run_examples_restores_a_non_executable_example_runner(
    tmp_path: Path,
) -> None:
    build_dir = tmp_path / "meson-debug"
    runner = _stub_example_runner(build_dir)
    assert not runner.stat().st_mode & stat.S_IXUSR

    run_examples(build_dir, examples=["Blink"], timeout=30)

    assert runner.stat().st_mode & stat.S_IXUSR, (
        "run_examples reached `meson test` with a non-executable example_runner"
    )
