"""Keep the standalone singleton scanner's declaration grammar aligned with Rust."""

import pytest

from ci.scan_singleton_elision import looks_like_var_def


@pytest.mark.parametrize("annotation", ["FL_NO_EXCEPT", "noexcept"])
def test_annotated_multiline_function_tail_is_not_storage(annotation: str) -> None:
    assert looks_like_var_def(f"u16 dx, u16 dy) {annotation};") == (False, "", "")


def test_real_namespace_storage_is_still_reported() -> None:
    is_definition, name, _type = looks_like_var_def("static Table global_state;")
    assert is_definition
    assert name == "global_state"
