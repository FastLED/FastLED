"""Regression coverage for build-host Python selection in Meson."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_meson_selects_host_python_from_build_machine() -> None:
    root_meson = (ROOT / "meson.build").read_text(encoding="utf-8")

    assert "build_machine.system() == 'windows'" in root_meson
    assert "host_python_name = 'python'" in root_meson
    assert "host_python_name = 'python3'" in root_meson
    assert (
        "host_python = find_program(host_python_name, required: true, native: true)"
        in root_meson
    )


def test_meson_host_helpers_reuse_normalized_python_program() -> None:
    root_meson = (ROOT / "meson.build").read_text(encoding="utf-8")
    tests_meson = (ROOT / "tests" / "meson.build").read_text(encoding="utf-8")

    assert "host_python, src_cache_script, '--check'" in root_meson
    assert "host_python, src_cache_script, '--update'" in root_meson
    assert "python_exe = host_python" in tests_meson
    assert "python_exe, cache_script, '--check'" in tests_meson
    assert "python_exe, meson.current_source_dir() / 'organize_tests.py'" in tests_meson
    assert "python_exe, cache_script, '--update'" in tests_meson
    assert "find_program('python')" not in root_meson
    assert "find_program('python')" not in tests_meson


def test_meson_host_python_commands_include_project_on_pythonpath() -> None:
    root_meson = (ROOT / "meson.build").read_text(encoding="utf-8")
    tests_meson = (ROOT / "tests" / "meson.build").read_text(encoding="utf-8")

    assert (
        "host_python_env.prepend('PYTHONPATH', meson.project_source_root())"
        in root_meson
    )
    assert root_meson.count("env: host_python_env") >= 2
    assert tests_meson.count("env: host_python_env") >= 3
