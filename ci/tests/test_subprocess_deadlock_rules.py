"""Tests for the subprocess ban's deadlock rules (SRC005) and spawn coverage.

`RunningProcess` is the only sanctioned way to spawn a child: it drains stdout
and stderr concurrently through an atomic queue. The stdlib API is banned, and
one shape of it is not merely discouraged but actively hangs:

    subprocess.Popen(cmd, stdout=PIPE, stderr=PIPE)   # only stdout read

Once the un-drained pipe fills its OS buffer (~64 KB Linux, ~8 KB Windows) the
child blocks forever and the parent waits forever. Verified empirically while
writing this rule: a child emitting 20k stderr lines under that shape never
returned, while the same child under `RunningProcess` finished in 0.3s.

SRC005 is therefore held at ZERO -- it has no baseline entries, so any new one
fails the build immediately. The broader codes ratchet down instead.
"""

from __future__ import annotations

from ci.lint_python.subprocess_capture_checker import check_file


def _codes(src: str) -> list[str]:
    return [code for _lineno, code, _msg in check_file("probe.py", src)]


class TestSrc005DeadlockShape:
    def test_both_pipes_undrained_is_flagged(self) -> None:
        src = (
            "import subprocess\n"
            "def go():\n"
            "    p = subprocess.Popen(['x'], stdout=subprocess.PIPE,\n"
            "                         stderr=subprocess.PIPE)\n"
            "    return p.stdout.readline()\n"
        )
        assert "SRC005" in _codes(src)

    def test_communicate_clears_it(self) -> None:
        """communicate() is the stdlib's own concurrent drain -- not a hang."""
        src = (
            "import subprocess\n"
            "def go():\n"
            "    p = subprocess.Popen(['x'], stdout=subprocess.PIPE,\n"
            "                         stderr=subprocess.PIPE)\n"
            "    out, err = p.communicate()\n"
            "    return out\n"
        )
        codes = _codes(src)
        assert "SRC005" not in codes
        # Still banned by the broad rule -- it is stdlib subprocess.
        assert "SRC002" in codes

    def test_stderr_redirected_to_stdout_is_not_flagged(self) -> None:
        """One merged pipe cannot leave a second one un-drained."""
        src = (
            "import subprocess\n"
            "def go():\n"
            "    p = subprocess.Popen(['x'], stdout=subprocess.PIPE,\n"
            "                         stderr=subprocess.STDOUT)\n"
            "    return p.stdout.readline()\n"
        )
        assert "SRC005" not in _codes(src)

    def test_single_pipe_is_not_flagged(self) -> None:
        src = (
            "import subprocess\n"
            "def go():\n"
            "    p = subprocess.Popen(['x'], stdout=subprocess.PIPE)\n"
            "    return p.stdout.readline()\n"
        )
        assert "SRC005" not in _codes(src)

    def test_devnull_is_not_flagged(self) -> None:
        """The fix applied to ci/tests/test_stale_lock_real_scenario.py."""
        src = (
            "import subprocess\n"
            "def go():\n"
            "    p = subprocess.Popen(['x'], stdout=subprocess.PIPE,\n"
            "                         stderr=subprocess.DEVNULL)\n"
            "    return p.stdout.readline()\n"
        )
        assert "SRC005" not in _codes(src)


class TestBanCoversEverySpawnApi:
    def test_non_capturing_run_is_flagged(self) -> None:
        """The ban is total: a run() that pipes nothing is still banned.

        This previously escaped -- SRC001 only fired when output was captured,
        so the majority of call sites were invisible to the ratchet.
        """
        src = "import subprocess\nsubprocess.run(['ls'])\n"
        assert "SRC001" in _codes(src)

    def test_call_is_flagged(self) -> None:
        src = "import subprocess\nsubprocess.call(['ls'])\n"
        assert "SRC003" in _codes(src)

    def test_check_output_is_flagged(self) -> None:
        src = "import subprocess\nsubprocess.check_output(['ls'])\n"
        assert "SRC003" in _codes(src)

    def test_os_system_is_flagged(self) -> None:
        src = "import os\nos.system('ls')\n"
        assert "SRC006" in _codes(src)

    def test_os_popen_is_flagged(self) -> None:
        src = "import os\nos.popen('ls').read()\n"
        assert "SRC006" in _codes(src)

    def test_unrelated_os_calls_are_not_flagged(self) -> None:
        src = "import os\nos.path.join('a', 'b')\nos.getcwd()\nos.environ.get('X')\n"
        assert _codes(src) == []

    def test_running_process_is_clean(self) -> None:
        """The sanctioned API must never be flagged."""
        src = (
            "from running_process import RunningProcess\n"
            "def go():\n"
            "    proc = RunningProcess(['ls'], auto_run=True)\n"
            "    return proc.wait()\n"
        )
        assert _codes(src) == []


class TestNoqaStillWorks:
    def test_noqa_suppresses(self) -> None:
        src = "import subprocess\nsubprocess.run(['ls'])  # noqa: SRC001\n"
        assert _codes(src) == []

    def test_noqa_suppresses_os_spawn(self) -> None:
        src = "import os\nos.system('ls')  # noqa: SRC006\n"
        assert _codes(src) == []
