# C++ Linter Architecture (ci/lint_cpp/)

**IMPORTANT: All C++ linters MUST use the centralized dispatcher for performance.**

## Central Dispatcher
- **File**: `ci/lint_cpp/run_all_checkers.py`
- Loads each file **ONCE** and runs all applicable checkers on it
- 10-100x faster than running standalone scripts on each file
- All checkers run in a single pass per scope (src/, examples/, tests/)

## Creating a New C++ Linter
1. Create a checker class in `ci/lint_cpp/your_checker.py` inheriting from `FileContentChecker`
2. Implement `should_process_file(file_path: str) -> bool` (filter which files to check)
3. Implement `check_file_content(file_content: FileContent) -> list[str]` (check logic)
4. Add checker instance to appropriate scope in `run_all_checkers.py` (see `create_checkers()`)
5. Checker now runs automatically via `bash lint --cpp`

## DO NOT
- Create standalone `test_*.py` scripts that scan files independently
- Call standalone scripts from the `lint` bash script
- Load files multiple times for different checks

## Reference Examples
- `ci/lint_cpp/serial_printf_checker.py` - Simple pattern matching checker
- `ci/lint_cpp/using_namespace_fl_in_examples_checker.py` - Regex-based checker
- `ci/lint_cpp/static_in_headers_checker.py` - Complex multi-condition checker
