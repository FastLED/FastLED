#!/usr/bin/env python3
"""Unit tests for NoexceptFunctionChecker (unified noexcept checker)."""

import unittest

from ci.lint_cpp.noexcept_checker import NoexceptFunctionChecker
from ci.util.check_files import FileContent


_FL_HEADER = "src/fl/log/log.h"
_PLATFORM_HEADER = "src/platforms/arm/d21/semaphore_samd.h"
_DRIVER_HEADER = "src/platforms/esp/32/drivers/rmt/some_driver.h"
_PLATFORM_SHARED = "src/platforms/shared/rx_device_native.h"
_PLATFORM_STUB = "src/platforms/stub/mutex_stub_noop.h"


def _make(code: str, path: str = _PLATFORM_HEADER) -> FileContent:
    return FileContent(path=path, content=code, lines=code.splitlines())


def _violations(code: str, path: str = _PLATFORM_HEADER) -> list[tuple[int, str]]:
    c = NoexceptFunctionChecker()
    fc = _make(code, path)
    if not c.should_process_file(path):
        return []
    c.check_file_content(fc)
    return c.violations.get(path, [])


# ── File filtering ──────────────────────────────────────────────────────────


class TestFileFiltering(unittest.TestCase):
    """Test that should_process_file correctly filters files."""

    # Files that SHOULD be processed
    def test_fl_header(self) -> None:
        self.assertTrue(NoexceptFunctionChecker().should_process_file(_FL_HEADER))

    def test_platform_header(self) -> None:
        self.assertTrue(NoexceptFunctionChecker().should_process_file(_PLATFORM_HEADER))

    def test_driver_header(self) -> None:
        self.assertTrue(NoexceptFunctionChecker().should_process_file(_DRIVER_HEADER))

    def test_platform_shared(self) -> None:
        self.assertTrue(NoexceptFunctionChecker().should_process_file(_PLATFORM_SHARED))

    def test_platform_stub(self) -> None:
        self.assertTrue(NoexceptFunctionChecker().should_process_file(_PLATFORM_STUB))

    def test_hpp_file(self) -> None:
        self.assertTrue(
            NoexceptFunctionChecker().should_process_file(
                "src/platforms/adafruit/clockless_real.hpp"
            )
        )

    # Files that should be SKIPPED
    def test_cpp_hpp_skipped(self) -> None:
        self.assertFalse(
            NoexceptFunctionChecker().should_process_file(
                "src/platforms/esp/32/condition_variable_esp32.cpp.hpp"
            )
        )

    def test_cpp_file_skipped(self) -> None:
        self.assertFalse(
            NoexceptFunctionChecker().should_process_file(
                "src/platforms/esp/32/foo.cpp"
            )
        )

    def test_noexcept_h_skipped(self) -> None:
        self.assertFalse(
            NoexceptFunctionChecker().should_process_file("src/fl/stl/noexcept.h")
        )

    def test_third_party_skipped(self) -> None:
        self.assertFalse(
            NoexceptFunctionChecker().should_process_file(
                "src/platforms/third_party/foo.h"
            )
        )

    def test_outside_src_skipped(self) -> None:
        self.assertFalse(
            NoexceptFunctionChecker().should_process_file("tests/test_foo.h")
        )

    def test_examples_skipped(self) -> None:
        self.assertFalse(
            NoexceptFunctionChecker().should_process_file("examples/Blink/Blink.h")
        )


# ── Should flag (missing noexcept) ──────────────────────────────────────────


class TestMissingNoexcept(unittest.TestCase):
    """Test that functions missing FL_NOEXCEPT are flagged."""

    def test_void_func(self) -> None:
        self.assertEqual(len(_violations("void foo();")), 1)

    def test_return_type(self) -> None:
        self.assertEqual(len(_violations("int bar(float x);")), 1)

    def test_const_method(self) -> None:
        self.assertEqual(len(_violations("bool isReady() const;")), 1)

    def test_definition(self) -> None:
        self.assertEqual(len(_violations("void foo() {")), 1)

    def test_class_method_def(self) -> None:
        self.assertEqual(len(_violations("void MyClass::doWork() {")), 1)

    def test_static_func(self) -> None:
        self.assertEqual(len(_violations("static void helper(int x);")), 1)

    def test_constructor(self) -> None:
        self.assertEqual(len(_violations("Semaphore();")), 1)

    def test_multiple(self) -> None:
        self.assertEqual(len(_violations("void a();\nint b();\nbool c() const;")), 3)

    def test_fl_scope(self) -> None:
        """Violations in src/fl/ are also detected."""
        self.assertEqual(len(_violations("void foo();", path=_FL_HEADER)), 1)


# ── Should pass (has noexcept / FL_NOEXCEPT) ────────────────────────────────


class TestHasNoexcept(unittest.TestCase):
    """Test that functions with noexcept/FL_NOEXCEPT are not flagged."""

    def test_noexcept(self) -> None:
        self.assertEqual(len(_violations("void foo() noexcept;")), 0)

    def test_noexcept_def(self) -> None:
        self.assertEqual(len(_violations("void foo() noexcept {")), 0)

    def test_noexcept_const(self) -> None:
        self.assertEqual(len(_violations("bool bar() const noexcept;")), 0)

    def test_fl_noexcept(self) -> None:
        self.assertEqual(len(_violations("void foo() FL_NOEXCEPT;")), 0)

    def test_fl_noexcept_def(self) -> None:
        self.assertEqual(len(_violations("void foo() FL_NOEXCEPT {")), 0)

    def test_fl_noexcept_const(self) -> None:
        self.assertEqual(len(_violations("bool bar() const FL_NOEXCEPT;")), 0)

    def test_fl_noexcept_override(self) -> None:
        self.assertEqual(len(_violations("void run() FL_NOEXCEPT override;")), 0)

    def test_noexcept_override(self) -> None:
        self.assertEqual(len(_violations("void run() noexcept override;")), 0)


# ── Exemptions ──────────────────────────────────────────────────────────────


class TestExemptions(unittest.TestCase):
    """Test that non-function patterns are not flagged."""

    def test_destructor(self) -> None:
        self.assertEqual(len(_violations("~Semaphore();")), 0)

    def test_destructor_qualified(self) -> None:
        self.assertEqual(len(_violations("Foo::~Foo() {")), 0)

    def test_deleted(self) -> None:
        self.assertEqual(len(_violations("Foo(const Foo&) = delete;")), 0)

    def test_defaulted(self) -> None:
        self.assertEqual(len(_violations("Foo() = default;")), 0)

    def test_pure_virtual(self) -> None:
        self.assertEqual(len(_violations("virtual void process() = 0;")), 0)

    def test_comment(self) -> None:
        self.assertEqual(len(_violations("// void foo();")), 0)

    def test_multiline_comment(self) -> None:
        self.assertEqual(len(_violations("/* void foo();\n*/")), 0)

    def test_suppression_ok_no_noexcept(self) -> None:
        self.assertEqual(len(_violations("void foo(); // ok no noexcept")), 0)

    def test_suppression_ok_no_fl_noexcept(self) -> None:
        self.assertEqual(len(_violations("void foo(); // ok no FL_NOEXCEPT")), 0)

    def test_suppression_noexcept_not_required(self) -> None:
        self.assertEqual(len(_violations("void foo(); // noexcept not required")), 0)

    def test_suppression_nolint(self) -> None:
        self.assertEqual(len(_violations("void foo(); // nolint")), 0)

    def test_macro_define(self) -> None:
        self.assertEqual(len(_violations("#define FOO(x) bar(x)")), 0)

    def test_if_statement(self) -> None:
        self.assertEqual(len(_violations("if (condition) {")), 0)

    def test_while_loop(self) -> None:
        self.assertEqual(len(_violations("while (running) {")), 0)

    def test_for_loop(self) -> None:
        self.assertEqual(len(_violations("for (int i = 0; i < n; i++) {")), 0)

    def test_switch(self) -> None:
        self.assertEqual(len(_violations("switch (state) {")), 0)

    def test_return_call(self) -> None:
        self.assertEqual(len(_violations("return getValue();")), 0)

    def test_typedef(self) -> None:
        self.assertEqual(len(_violations("typedef void (*cb_t)(int);")), 0)

    def test_using(self) -> None:
        self.assertEqual(len(_violations("using cb_t = void(*)(int);")), 0)

    def test_macro_call_allcaps(self) -> None:
        self.assertEqual(len(_violations('FL_WARN("msg");')), 0)

    def test_macro_fl_assert(self) -> None:
        self.assertEqual(len(_violations('FL_ASSERT(false, "bad");')), 0)

    def test_macro_esp_error(self) -> None:
        self.assertEqual(len(_violations("ESP_ERROR_CHECK(ret);")), 0)

    def test_operator_overload(self) -> None:
        self.assertEqual(len(_violations("bool operator==(const Foo& o) const;")), 0)

    def test_operator_call(self) -> None:
        self.assertEqual(len(_violations("void operator()(T* ptr) const {")), 0)

    def test_ternary_continuation(self) -> None:
        self.assertEqual(len(_violations(": foo(x, y);")), 0)

    def test_static_cast(self) -> None:
        self.assertEqual(len(_violations("auto x = static_cast<int>(y);")), 0)

    def test_asm_volatile(self) -> None:
        self.assertEqual(len(_violations('asm volatile("nop");')), 0)

    def test_asm_dunder_volatile(self) -> None:
        self.assertEqual(len(_violations('asm __volatile__ ("nop");')), 0)

    def test_initializer_list_continuation(self) -> None:
        self.assertEqual(len(_violations(", mFoo(false) {")), 0)

    def test_lambda_expression(self) -> None:
        self.assertEqual(len(_violations("fl::thread t([ctx_shared, func]() {")), 0)

    def test_nvic_call(self) -> None:
        self.assertEqual(len(_violations("NVIC_EnableIRQ(handle_data->timer_irq);")), 0)

    def test_nvic_set_priority(self) -> None:
        self.assertEqual(
            len(_violations("NVIC_SetPriority(handle_data->timer_irq, 0);")), 0
        )


# ── Realistic patterns ─────────────────────────────────────────────────────


class TestRealistic(unittest.TestCase):
    """Test realistic code patterns."""

    def test_class_missing(self) -> None:
        code = """\
class Semaphore {
    void lock();
    void unlock();
    bool tryLock() const;
};"""
        self.assertEqual(len(_violations(code)), 3)

    def test_class_with_noexcept(self) -> None:
        code = """\
class Semaphore {
    void lock() FL_NOEXCEPT;
    void unlock() FL_NOEXCEPT;
    bool tryLock() const FL_NOEXCEPT;
};"""
        self.assertEqual(len(_violations(code)), 0)

    def test_mixed(self) -> None:
        code = """\
class Foo {
    Foo();
    ~Foo();
    Foo(const Foo&) = delete;
    void go() FL_NOEXCEPT;
    void stop();
};"""
        # Foo() constructor + stop() = 2 violations
        self.assertEqual(len(_violations(code)), 2)

    def test_esp32_driver(self) -> None:
        """ESP32 driver code is also checked."""
        code = """\
class ChannelEngineSpi {
    void show();
    void poll();
};"""
        self.assertEqual(len(_violations(code, path=_DRIVER_HEADER)), 2)


if __name__ == "__main__":
    unittest.main()
