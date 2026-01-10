"""Flake8 plugin to check for proper KeyboardInterrupt handling.

This plugin ensures that try-except blocks that catch broad exceptions
(like Exception or BaseException) also properly handle KeyboardInterrupt.

Error Codes:
    KBI001: Try-except catches Exception/BaseException without KeyboardInterrupt handler
    KBI002: KeyboardInterrupt handler must call _thread.interrupt_main() or handle_keyboard_interrupt_properly()
"""

import ast
from typing import Any, Generator, Tuple, Type


class KeyboardInterruptChecker:
    """Flake8 plugin to check for proper KeyboardInterrupt handling."""

    name = "keyboard-interrupt-checker"
    version = "1.0.0"

    def __init__(self, tree: ast.AST) -> None:
        """Initialize the checker with an AST tree.

        Args:
            tree: The AST tree to check
        """
        self._tree = tree

    def run(self) -> Generator[Tuple[int, int, str, Type[Any]], None, None]:
        """Run the checker on the AST tree.

        Yields:
            Tuple of (line_number, column, message, type)
        """
        visitor = TryExceptVisitor()
        visitor.visit(self._tree)

        for line, col, msg in visitor.errors:
            yield (line, col, msg, type(self))


class TryExceptVisitor(ast.NodeVisitor):
    """AST visitor to check try-except blocks for KeyboardInterrupt handling."""

    def __init__(self) -> None:
        """Initialize the visitor."""
        self.errors: list[Tuple[int, int, str]] = []

    def visit_Try(self, node: ast.Try) -> None:
        """Visit a Try node and check exception handlers.

        Args:
            node: The Try node to check
        """
        # Check if any handler catches Exception or BaseException
        catches_broad_exception = False
        has_keyboard_interrupt_handler = False
        keyboard_interrupt_handlers = []

        for handler in node.handlers:
            if handler.type is None:
                # bare except: catches everything
                catches_broad_exception = True
            elif isinstance(handler.type, ast.Name):
                if handler.type.id in ("Exception", "BaseException"):
                    catches_broad_exception = True
                elif handler.type.id == "KeyboardInterrupt":
                    has_keyboard_interrupt_handler = True
                    keyboard_interrupt_handlers.append(handler)
            elif isinstance(handler.type, ast.Tuple):
                # Check if tuple contains Exception or BaseException
                for exc_type in handler.type.elts:
                    if isinstance(exc_type, ast.Name):
                        if exc_type.id in ("Exception", "BaseException"):
                            catches_broad_exception = True
                        elif exc_type.id == "KeyboardInterrupt":
                            has_keyboard_interrupt_handler = True
                            keyboard_interrupt_handlers.append(handler)

        # If we catch broad exceptions without KeyboardInterrupt handler, that's an error
        if catches_broad_exception and not has_keyboard_interrupt_handler:
            self.errors.append(
                (
                    node.lineno,
                    node.col_offset,
                    (
                        "KBI001 Try-except catches Exception/BaseException without KeyboardInterrupt handler. "
                        "Add: except KeyboardInterrupt as ke: handle_keyboard_interrupt_properly(ke)"
                    ),
                )
            )

        # Check all KeyboardInterrupt handlers to ensure they call _thread.interrupt_main()
        for keyboard_interrupt_handler in keyboard_interrupt_handlers:
            if not self._handler_calls_interrupt_main(keyboard_interrupt_handler):
                self.errors.append(
                    (
                        keyboard_interrupt_handler.lineno,
                        keyboard_interrupt_handler.col_offset,
                        (
                            "KBI002 KeyboardInterrupt handler must call _thread.interrupt_main() "
                            "or use handle_keyboard_interrupt_properly(). "
                            "Add: import _thread; _thread.interrupt_main()"
                        ),
                    )
                )

        # Continue visiting child nodes
        self.generic_visit(node)

    def _handler_calls_interrupt_main(self, handler: ast.ExceptHandler) -> bool:
        """Check if a KeyboardInterrupt handler properly calls _thread.interrupt_main().

        Args:
            handler: The exception handler to check

        Returns:
            True if the handler calls _thread.interrupt_main() or handle_keyboard_interrupt_properly()
        """
        # Check for calls to _thread.interrupt_main() or handle_keyboard_interrupt_properly()
        for node in ast.walk(handler):
            if isinstance(node, ast.Call):
                # Check for _thread.interrupt_main()
                if isinstance(node.func, ast.Attribute):
                    if (
                        isinstance(node.func.value, ast.Name)
                        and node.func.value.id == "_thread"
                        and node.func.attr == "interrupt_main"
                    ):
                        return True

                # Check for handle_keyboard_interrupt_properly() or notify_main_thread()
                if isinstance(node.func, ast.Name):
                    if node.func.id in ("handle_keyboard_interrupt_properly", "notify_main_thread"):
                        return True

        return False
