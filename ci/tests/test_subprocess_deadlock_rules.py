"""The stdlib `subprocess` module is banned outright in favour of RunningProcess.

These tests pin the rules of ci/lint_python/subprocess_capture_checker.py: every
spelling of the stdlib API fails (calls, imports, attribute references), the
os.system/os.popen shell-outs fail, and a capturing RunningProcess call in
text mode must name its encoding. There is no baseline and no noqa escape.
"""

from __future__ import annotations

import textwrap
from pathlib import Path

from ci.lint_python.subprocess_capture_checker import check_file, scan


def _codes(source: str) -> list[str]:
    return [code for _, code, _ in check_file("t.py", textwrap.dedent(source))]


class TestStdlibCallsAreBanned:
    def test_run(self) -> None:
        src = """
        import subprocess
        subprocess.run(["ls"])
        """
        assert _codes(src) == ["SRC007", "SRC001", "SRC007"]

    def test_popen(self) -> None:
        assert "SRC002" in _codes("subprocess.Popen(['ls'])\n")

    def test_convenience_wrappers(self) -> None:
        for name in ("check_output", "check_call", "call", "getoutput"):
            assert "SRC003" in _codes(f"subprocess.{name}('ls')\n"), name

    def test_capture_without_encoding_in_text_mode(self) -> None:
        src = "subprocess.run(['ls'], capture_output=True, text=True)\n"
        assert "SRC004" in _codes(src)

    def test_capture_with_encoding_has_no_src004(self) -> None:
        src = (
            'subprocess.run(["ls"], capture_output=True, text=True, encoding="utf-8")\n'
        )
        assert "SRC004" not in _codes(src)


class TestModuleReferencesAreBanned:
    def test_import(self) -> None:
        assert _codes("import subprocess\n") == ["SRC007"]

    def test_from_import(self) -> None:
        assert _codes("from subprocess import PIPE, CalledProcessError\n") == ["SRC007"]

    def test_attribute_reference(self) -> None:
        src = """
        def f(err):
            return isinstance(err, subprocess.TimeoutExpired)
        """
        assert _codes(src) == ["SRC007"]

    def test_one_report_per_line_for_module_reference(self) -> None:
        src = "x = (subprocess.PIPE, subprocess.STDOUT)\n"
        assert _codes(src) == ["SRC007"]


class TestOsSpawns:
    def test_os_system_is_flagged(self) -> None:
        assert _codes("os.system('ls')\n") == ["SRC006"]

    def test_os_popen_is_flagged(self) -> None:
        assert _codes("os.popen('ls').read()\n") == ["SRC006"]

    def test_unrelated_os_calls_are_not_flagged(self) -> None:
        assert _codes("os.getcwd()\nos.path.join('a', 'b')\n") == []


class TestRunningProcess:
    def test_clean_usage_passes(self) -> None:
        src = """
        from running_process import RunningProcess, PIPE
        RunningProcess.run(["ls"], capture_output=True, encoding="utf-8", errors="replace")
        RunningProcess(["ls"], auto_run=False, capture=True, encoding="utf-8")
        RunningProcess.run(["ls"], stdout=PIPE, stderr=PIPE, encoding="utf-8")
        RunningProcess.run(["ls"])
        """
        assert _codes(src) == []

    def test_capture_without_encoding_is_flagged(self) -> None:
        assert _codes('RunningProcess.run(["ls"], capture_output=True)\n') == ["SRC004"]
        assert _codes('RunningProcess(["ls"], capture=True)\n') == ["SRC004"]
        assert _codes('RunningProcess.run(["ls"], stdout=PIPE)\n') == ["SRC004"]

    def test_bytes_capture_needs_no_encoding(self) -> None:
        src = 'RunningProcess.run(["ls"], capture_output=True, text=False)\n'
        assert _codes(src) == []


class TestNoEscapeHatch:
    def test_noqa_does_not_suppress(self) -> None:
        assert "SRC001" in _codes("subprocess.run(['ls'])  # noqa: SRC001\n")
        assert "SRC006" in _codes("os.system('ls')  # noqa\n")

    def test_strings_and_comments_are_not_findings(self) -> None:
        src = """
        # subprocess.run is banned; RunningProcess.run replaces it
        MESSAGE = "use RunningProcess instead of subprocess.run(...)"
        """
        assert _codes(src) == []


def test_scan_reports_relative_paths_and_skips_excluded_dirs(tmp_path: Path) -> None:
    (tmp_path / "ci").mkdir()
    (tmp_path / "ci" / "bad.py").write_text("import subprocess\n", encoding="utf-8")
    (tmp_path / ".venv").mkdir()
    (tmp_path / ".venv" / "lib.py").write_text("import subprocess\n", encoding="utf-8")
    (tmp_path / "ok.py").write_text("print('fine')\n", encoding="utf-8")

    findings = scan([str(tmp_path)], [".venv"])

    assert len(findings) == 1
    assert findings[0].endswith("ci/bad.py:1: " + _message("SRC007"))


def _message(code: str) -> str:
    from ci.lint_python.subprocess_capture_checker import _MESSAGES

    return _MESSAGES[code]
