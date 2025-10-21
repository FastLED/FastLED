"""Tests for sketch filter parsing and evaluation.

Tests the enhanced @filter syntax supporting:
- Natural language operators (is, =, :)
- Property name shortcuts (mem, plat, tgt, brd)
- Optional colon after @filter
- Compound filters with and/or logic
"""

import tempfile
from pathlib import Path

import pytest

from ci.boards import Board
from ci.compiler.sketch_filter import (
    SketchFilter,
    _normalize_property_name,
    parse_filter_from_sketch,
    parse_oneline_filter,
    should_skip_sketch,
)


class TestNormalizePropertyName:
    """Test property name normalization."""

    def test_full_names_unchanged(self):
        """Full property names should pass through unchanged."""
        assert _normalize_property_name("memory") == "memory"
        assert _normalize_property_name("platform") == "platform"
        assert _normalize_property_name("target") == "target"
        assert _normalize_property_name("board") == "board"

    def test_shorthand_names(self):
        """Shorthand names should be expanded to full names."""
        assert _normalize_property_name("mem") == "memory"
        assert _normalize_property_name("plat") == "platform"
        assert _normalize_property_name("tgt") == "target"
        assert _normalize_property_name("brd") == "board"

    def test_case_insensitive(self):
        """Property names should be case-insensitive."""
        assert _normalize_property_name("MEM") == "memory"
        assert _normalize_property_name("PLAT") == "platform"
        assert _normalize_property_name("Memory") == "memory"
        assert _normalize_property_name("PLATFORM") == "platform"

    def test_whitespace_handling(self):
        """Should trim whitespace."""
        assert _normalize_property_name("  mem  ") == "memory"
        assert _normalize_property_name("\tplat\n") == "platform"


class TestParseOnelineFilterOperators:
    """Test one-liner filter parsing with different operator styles."""

    def test_is_operator(self):
        """Test 'is' operator."""
        result = parse_oneline_filter("(memory is high)")
        assert result is not None
        assert result.require == {"memory": ["high"]}
        assert result.exclude == {}

    def test_is_not_operator(self):
        """Test 'is not' operator."""
        result = parse_oneline_filter("(memory is not low)")
        assert result is not None
        assert result.require == {}
        assert result.exclude == {"memory": ["low"]}

    def test_equals_operator(self):
        """Test '=' operator as shorthand for 'is'."""
        result = parse_oneline_filter("(mem=high)")
        assert result is not None
        assert result.require == {"memory": ["high"]}
        assert result.exclude == {}

    def test_colon_operator(self):
        """Test ':' operator as shorthand for 'is'."""
        result = parse_oneline_filter("(mem:high)")
        assert result is not None
        assert result.require == {"memory": ["high"]}
        assert result.exclude == {}

    def test_all_operator_styles_equivalent(self):
        """All three operator styles should produce equivalent results."""
        filters = [
            "(memory is high)",
            "(mem is high)",
            "(mem=high)",
            "(mem:high)",
        ]
        results = [parse_oneline_filter(f) for f in filters]

        # All should parse successfully
        assert all(r is not None for r in results)

        # First result (reference)
        ref = results[0]

        # All should be equivalent
        for result in results[1:]:
            assert result.require == ref.require
            assert result.exclude == ref.exclude


class TestParseOnelineFilterShortcuts:
    """Test property name shortcuts in filters."""

    def test_memory_shortcut(self):
        """Test 'mem' shortcut for 'memory'."""
        result = parse_oneline_filter("(mem is high)")
        assert result is not None
        assert "memory" in result.require
        assert result.require["memory"] == ["high"]

    def test_platform_shortcut(self):
        """Test 'plat' shortcut for 'platform'."""
        result = parse_oneline_filter("(plat is esp32)")
        assert result is not None
        assert "platform" in result.require
        assert result.require["platform"] == ["esp32"]

    def test_target_shortcut(self):
        """Test 'tgt' shortcut for 'target'."""
        result = parse_oneline_filter("(tgt is ESP32S3)")
        assert result is not None
        assert "target" in result.require
        assert result.require["target"] == ["ESP32S3"]

    def test_board_shortcut(self):
        """Test 'brd' shortcut for 'board'."""
        result = parse_oneline_filter("(brd is uno)")
        assert result is not None
        assert "board" in result.require
        assert result.require["board"] == ["uno"]


class TestParseOnelineFilterCompound:
    """Test compound filters with and/or logic."""

    def test_and_logic(self):
        """Test 'and' operator combining multiple conditions."""
        result = parse_oneline_filter("(mem is high) and (plat is esp32)")
        assert result is not None
        assert result.require == {
            "memory": ["high"],
            "platform": ["esp32"],
        }

    def test_or_in_exclude(self):
        """Test 'or' logic with negation (exclude)."""
        result = parse_oneline_filter("(plat is not esp8266) and (plat is not avr)")
        assert result is not None
        assert result.exclude == {
            "platform": ["esp8266", "avr"],
        }

    def test_mixed_requires_and_excludes(self):
        """Test mixture of require and exclude conditions."""
        result = parse_oneline_filter("(mem is high) and (plat is not avr)")
        assert result is not None
        assert result.require == {"memory": ["high"]}
        assert result.exclude == {"platform": ["avr"]}

    def test_complex_filter(self):
        """Test complex filter with multiple conditions."""
        result = parse_oneline_filter(
            "(mem is high) and (plat is esp32*) and (tgt is not esp32p4)"
        )
        assert result is not None
        assert result.require == {
            "memory": ["high"],
            "platform": ["esp32*"],
        }
        assert result.exclude == {"target": ["esp32p4"]}


class TestParseOnelineFilterWildcards:
    """Test wildcard and glob patterns."""

    def test_glob_pattern(self):
        """Test glob pattern matching."""
        result = parse_oneline_filter("(plat is esp32*)")
        assert result is not None
        assert result.require == {"platform": ["esp32*"]}

    def test_cpp_define_format(self):
        """Test C++ define format (-D prefix)."""
        result = parse_oneline_filter("(tgt is -D__AVR__)")
        assert result is not None
        assert result.require == {"target": ["-D__AVR__"]}


class TestParseOnelineFilterQuotes:
    """Test quoted values in filters."""

    def test_double_quotes(self):
        """Test double-quoted values."""
        result = parse_oneline_filter('(board is "uno")')
        assert result is not None
        assert result.require == {"board": ["uno"]}

    def test_single_quotes(self):
        """Test single-quoted values."""
        result = parse_oneline_filter("(board is 'uno')")
        assert result is not None
        assert result.require == {"board": ["uno"]}


class TestParseFilterFromSketchOneliners:
    """Test parsing @filter directives from sketch files."""

    def _write_and_parse(self, sketch_content):
        """Helper to write sketch content and parse it."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".ino", delete=False) as f:
            f.write(sketch_content)
            f.flush()
            temp_path = f.name

        try:
            return parse_filter_from_sketch(Path(temp_path))
        finally:
            Path(temp_path).unlink(missing_ok=True)

    def test_filter_with_colon(self):
        """Test @filter: syntax with colon."""
        sketch_content = "// @filter: (mem is high)"
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {"memory": ["high"]}

    def test_filter_without_colon(self):
        """Test @filter syntax without colon."""
        sketch_content = "// @filter (mem is high)"
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {"memory": ["high"]}

    def test_filter_with_equals(self):
        """Test @filter with = operator."""
        sketch_content = "// @filter (mem=high)"
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {"memory": ["high"]}

    def test_filter_with_colon_operator(self):
        """Test @filter with : operator."""
        sketch_content = "// @filter (mem:high)"
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {"memory": ["high"]}

    def test_filter_compound_logic(self):
        """Test compound filter with multiple conditions."""
        sketch_content = (
            "// @filter (mem is high) and (plat is esp32*) and (tgt is not esp32p4)"
        )
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {
            "memory": ["high"],
            "platform": ["esp32*"],
        }
        assert result.exclude == {"target": ["esp32p4"]}

    def test_no_filter(self):
        """Test sketch without @filter."""
        sketch_content = "// Regular comment\nvoid setup() {}"
        result = self._write_and_parse(sketch_content)
        assert result is None


class TestParseFilterFromSketchYAML:
    """Test parsing YAML-style multi-line @filter blocks."""

    def _write_and_parse(self, sketch_content):
        """Helper to write sketch content and parse it."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".ino", delete=False) as f:
            f.write(sketch_content)
            f.flush()
            temp_path = f.name

        try:
            return parse_filter_from_sketch(Path(temp_path))
        finally:
            Path(temp_path).unlink(missing_ok=True)

    def test_yaml_with_colon(self):
        """Test YAML-style @filter: with colon."""
        sketch_content = """// @filter:
// require:
//   - memory: high
//   - platform: esp32
// @end-filter
"""
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {
            "memory": ["high"],
            "platform": ["esp32"],
        }

    def test_yaml_without_colon(self):
        """Test YAML-style @filter without colon."""
        sketch_content = """// @filter
// require:
//   - memory: high
//   - platform: esp32
// @end-filter
"""
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {
            "memory": ["high"],
            "platform": ["esp32"],
        }

    def test_yaml_with_shortcuts(self):
        """Test YAML-style using property shortcuts."""
        sketch_content = """// @filter
// require:
//   - mem: high
//   - plat: esp32
// @end-filter
"""
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {
            "memory": ["high"],
            "platform": ["esp32"],
        }

    def test_yaml_with_exclude(self):
        """Test YAML-style with exclude section."""
        sketch_content = """// @filter
// require:
//   - memory: high
// exclude:
//   - platform: avr
// @end-filter
"""
        result = self._write_and_parse(sketch_content)
        assert result is not None
        assert result.require == {"memory": ["high"]}
        assert result.exclude == {"platform": ["avr"]}


class TestShouldSkipSketch:
    """Test sketch skip logic based on board and filter."""

    @pytest.fixture
    def esp32_board(self):
        """Create a mock ESP32 board."""
        from unittest.mock import MagicMock

        board = MagicMock()
        board.platform_family = "esp32"
        board.memory_class = "high"
        board.get_mcu_target = MagicMock(return_value="ESP32")
        return board

    @pytest.fixture
    def avr_board(self):
        """Create a mock AVR board (Arduino Uno)."""
        from unittest.mock import MagicMock

        board = MagicMock()
        board.platform_family = "avr"
        board.memory_class = "low"
        board.get_mcu_target = MagicMock(return_value="ATmega328P")
        return board

    def test_no_filter_should_not_skip(self, esp32_board):
        """Sketch without filter should not be skipped."""
        should_skip, reason = should_skip_sketch(esp32_board, None)
        assert not should_skip
        assert reason == ""

    def test_empty_filter_should_not_skip(self, esp32_board):
        """Empty filter should not skip."""
        filter_obj = SketchFilter()
        should_skip, reason = should_skip_sketch(esp32_board, filter_obj)
        assert not should_skip
        assert reason == ""

    def test_require_high_memory_on_esp32(self, esp32_board):
        """ESP32 (high memory) should not be skipped for high memory requirement."""
        filter_obj = SketchFilter(require={"memory": ["high"]})
        should_skip, reason = should_skip_sketch(esp32_board, filter_obj)
        assert not should_skip

    def test_require_high_memory_on_avr(self, avr_board):
        """AVR (low memory) should be skipped for high memory requirement."""
        filter_obj = SketchFilter(require={"memory": ["high"]})
        should_skip, reason = should_skip_sketch(avr_board, filter_obj)
        assert should_skip
        assert "high memory" in reason or "memory" in reason

    def test_require_platform_esp32(self, esp32_board, avr_board):
        """ESP32 should compile, AVR should skip."""
        filter_obj = SketchFilter(require={"platform": ["esp32"]})

        should_skip_esp32, _ = should_skip_sketch(esp32_board, filter_obj)
        should_skip_avr, _ = should_skip_sketch(avr_board, filter_obj)

        assert not should_skip_esp32
        assert should_skip_avr

    def test_exclude_platform_avr(self, esp32_board, avr_board):
        """AVR should be excluded, ESP32 should compile."""
        filter_obj = SketchFilter(exclude={"platform": ["avr"]})

        should_skip_esp32, _ = should_skip_sketch(esp32_board, filter_obj)
        should_skip_avr, _ = should_skip_sketch(avr_board, filter_obj)

        assert not should_skip_esp32
        assert should_skip_avr

    def test_glob_pattern_platform(self, esp32_board, avr_board):
        """Glob pattern should match ESP32 variants."""
        filter_obj = SketchFilter(require={"platform": ["esp32*"]})

        should_skip_esp32, _ = should_skip_sketch(esp32_board, filter_obj)
        should_skip_avr, _ = should_skip_sketch(avr_board, filter_obj)

        assert not should_skip_esp32
        assert should_skip_avr


class TestEdgeCases:
    """Test edge cases and error handling."""

    def test_empty_filter_line(self):
        """Empty filter line should return None."""
        result = parse_oneline_filter("")
        assert result is None

    def test_whitespace_only(self):
        """Whitespace-only filter should return None."""
        result = parse_oneline_filter("   ")
        assert result is None

    def test_invalid_filter_syntax(self):
        """Invalid syntax should return None."""
        result = parse_oneline_filter("memory high")  # Missing parentheses
        assert result is None

    def test_unknown_property(self):
        """Unknown property should be ignored."""
        result = parse_oneline_filter("(unknown is value)")
        assert result is None  # No valid properties found

    def test_unmatched_parenthesis(self):
        """Unmatched parenthesis should be handled gracefully."""
        result = parse_oneline_filter("(memory is high")
        assert result is None  # Regex won't match

    def test_multiple_filters_only_first_used(self):
        """File with multiple @filter directives uses first match."""
        sketch_content = """
// @filter (mem is high)
some code
// @filter (plat is avr)
"""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".ino", delete=False) as f:
            f.write(sketch_content)
            f.flush()
            temp_path = f.name

        try:
            result = parse_filter_from_sketch(Path(temp_path))
            assert result is not None
            # Should use the first @filter found
            assert result.require == {"memory": ["high"]}
        finally:
            Path(temp_path).unlink(missing_ok=True)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
