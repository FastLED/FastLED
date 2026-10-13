"""Real clang-query parsing must work beyond Windows' command-line limit."""

import json
from pathlib import Path
from typing import Any

import pytest

from ci.tools import check_ast_combined, check_noexcept


@pytest.mark.parametrize("failure", [None, "compiler", "spawn"])
def test_long_profile_arguments_use_temporary_compilation_database(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    failure: str | None,
) -> None:
    query = check_noexcept._find_clang_query()
    assert query, "required clang-query is unavailable"
    monkeypatch.setattr(check_ast_combined, "PROJECT_ROOT", tmp_path)
    source_dir = tmp_path / "src"
    source_dir.mkdir()
    include_dir = tmp_path / "SDK headers with spaces"
    include_dir.mkdir()
    header = include_dir / "backend.h"
    header.write_text(
        "namespace fl { struct PortableBackend { void read() noexcept {} }; }\n",
        encoding="utf-8",
    )
    source = source_dir / "profile.cpp"
    source.write_text(
        "#include <initializer_list>\n"
        '#include "backend.h"\n'
        "std::initializer_list<int> values = {1, 2};\n"
        "constexpr char text[] = QUOTED_TEXT;\n"
        r"static_assert(text[8] == '\\' && text[13] == '\"');"
        "\n"
        + ("#error deliberate compiler failure\n" if failure == "compiler" else ""),
        encoding="utf-8",
    )
    arguments = check_noexcept._compiler_args(
        [
            "-std=c++17",
            "-I",
            str(include_dir),
            r'-DQUOTED_TEXT="sdk path\\with\"quote"',
            *[f"-DPADDING_{index}=" + "x" * 80 for index in range(500)],
        ]
    )
    assert sum(len(argument) + 1 for argument in arguments) > 32767
    original_run = check_ast_combined.RunningProcess.run
    databases: list[Path] = []

    def bounded_spawn(command: list[str], **kwargs: Any) -> Any:
        # Enforce the failing Windows boundary on every test host, then let
        # the installed LLVM parse the actual database and compiler arguments.
        assert (
            sum(len(argument.encode("utf-16-le")) // 2 + 3 for argument in command)
            < 32767
        )
        database = Path(command[command.index("-p") + 1]) / "compile_commands.json"
        databases.append(database)
        entries = json.loads(database.read_text(encoding="utf-8"))
        assert len(entries) == 1
        assert entries[0]["directory"] == str(tmp_path)
        assert entries[0]["file"] == str(source)
        assert Path(entries[0]["arguments"][0]).is_absolute()
        assert entries[0]["arguments"][1:] == [
            *arguments,
            "-H",
            "-fshow-skipped-includes",
            str(source),
        ]
        assert kwargs["env"] is environment
        if failure == "spawn":
            raise OSError("simulated spawn failure")
        return original_run(command, **kwargs)

    environment = dict(check_ast_combined.os.environ)
    monkeypatch.setattr(check_ast_combined.RunningProcess, "run", bounded_spawn)

    def analyze() -> check_ast_combined.TranslationUnitAnalysis:
        return check_ast_combined.analyze_translation_unit(
            query,
            str(source),
            ".*",
            arguments,
            expected_records=("fl::PortableBackend",),
            process_env=environment,
        )

    if failure == "compiler":
        with pytest.raises(
            check_noexcept.NoexceptCheckError, match="deliberate compiler failure"
        ):
            analyze()
    elif failure == "spawn":
        with pytest.raises(OSError, match="simulated spawn failure"):
            analyze()
    else:
        result = analyze()
        assert "fl::PortableBackend" in result.present_records
        assert {source, header} <= set(result.dependencies)
        assert any(path.name == "initializer_list" for path in result.dependencies)
        assert not result.noexcept_hits

    assert len(databases) == 1
    assert not databases[0].parent.exists(), "temporary compiler arguments leaked"
