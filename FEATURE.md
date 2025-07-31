# Test Runner Refactoring Plan

## Current Issues

The current test execution system in `test.py` has several issues:

1. **Duplicated Logic**: Multiple functions implement similar test running patterns with slight variations
2. **Inconsistent Process Management**: Some functions run processes sequentially, others in parallel
3. **Ad-hoc Output Handling**: Various approaches to handling process output
4. **Mixed Responsibilities**: Functions both create processes and manage their execution
5. **Scattered Test Configuration**: Test setup logic is spread across multiple functions

## Refactoring Goals

1. **Centralize Process Execution**: Create a unified runner for all test processes
2. **Separate Process Creation from Execution**: Process creation functions should only return RunningProcess objects
3. **Standardize Output Handling**: Consistent approach to capturing and displaying process output
4. **Improve Testability**: Make components more modular for easier testing
5. **Maintain Existing Functionality**: Preserve all current features and behaviors

## Architecture Changes

### 1. Process Factory Functions

Convert all process creation functions to return RunningProcess objects without starting them:

```python
def create_unit_test_process(args: TestArgs, enable_stack_trace: bool) -> RunningProcess:
    """Create a unit test process without starting it"""
    cmd_str_cpp = build_cpp_test_command(args)
    # GCC builds are 5x slower due to poor unified compilation performance
    cpp_test_timeout = 900 if args.gcc else 300
    return RunningProcess(
        cmd_str_cpp,
        enable_stack_trace=enable_stack_trace,
        timeout=cpp_test_timeout,
        auto_run=False,  # Never auto-run
    )
```

### 2. Process Collection Functions

Create functions that return collections of processes to run:

```python
def get_cpp_test_processes(args: TestArgs, test_categories: TestCategories, enable_stack_trace: bool) -> list[RunningProcess]:
    """Return all processes needed for C++ tests"""
    processes = []
    
    # Always include namespace check
    processes.append(create_namespace_check_process(enable_stack_trace))
    
    if test_categories.unit:
        processes.append(create_unit_test_process(args, enable_stack_trace))
        
    if test_categories.examples:
        processes.append(create_examples_test_process(args, enable_stack_trace))
        
    return processes
```

### 3. Centralized Runner

Create a unified runner that handles all process execution:

```python
def run_test_processes(processes: list[RunningProcess], parallel: bool = True) -> None:
    """
    Run multiple test processes and handle their output
    
    Args:
        processes: List of RunningProcess objects to execute
        parallel: Whether to run processes in parallel or sequentially
    """
    if parallel:
        # Run all processes in parallel with output interleaving
        _run_processes_parallel(processes)
    else:
        # Run processes sequentially with full output capture
        for process in processes:
            _run_process_with_output(process)
```

### 4. Main Runner Function

Create a main runner function that orchestrates the entire test execution:

```python
def runner(args: TestArgs) -> None:
    """
    Main test runner function that determines what to run and executes tests
    
    Args:
        args: Parsed command line arguments
    """
    # Determine test categories
    test_categories = determine_test_categories(args)
    enable_stack_trace = not args.no_stack_trace
    
    # Get processes to run based on test categories
    processes = []
    
    if test_categories.unit_only:
        processes = get_unit_test_processes(args, enable_stack_trace)
        parallel = False
    elif test_categories.examples_only:
        processes = get_examples_test_processes(args, enable_stack_trace)
        parallel = False
    # ... other combinations
    else:
        # All tests or complex combinations
        processes = get_all_test_processes(args, test_categories, enable_stack_trace)
        parallel = True
    
    # Run the processes
    run_test_processes(processes, parallel=parallel)
```

## Implementation Plan

1. **Phase 1: Process Factory Refactoring**
   - Convert all process creation functions to return RunningProcess objects
   - Ensure consistent parameter passing and configuration

2. **Phase 2: Process Collection Functions**
   - Create functions that return collections of processes for different test scenarios
   - Move logic from existing run_* functions to these collection functions

3. **Phase 3: Centralized Runner**
   - Implement the unified runner for process execution
   - Support both parallel and sequential execution modes
   - Standardize output handling and error reporting

4. **Phase 4: Main Runner Integration**
   - Create the main runner function
   - Update main() to use the new runner
   - Ensure backward compatibility with all command line options

5. **Phase 5: Testing and Validation**
   - Verify all test combinations work as expected
   - Ensure output formatting is consistent
   - Validate error handling and reporting

## Benefits

1. **Improved Maintainability**: Centralized process execution logic
2. **Better Testability**: Isolated components are easier to test
3. **Consistent User Experience**: Standardized output handling
4. **Easier Extensions**: Adding new test types becomes simpler
5. **Reduced Code Duplication**: Eliminates redundant process management code

## Migration Strategy

1. Implement the new architecture alongside the existing code
2. Gradually migrate each test type to the new system
3. Validate each migration with comprehensive testing
4. Once all test types are migrated, remove the old execution code
5. Maintain backward compatibility with existing command line arguments

## Detailed Component Specifications

### TestProcessConfig Class

Enhance the existing TestProcessConfig class to include more configuration options:

```python
@dataclass
class TestProcessConfig:
    """Configuration for a test process"""
    command: str | list[str]
    echo: bool = True
    auto_run: bool = False
    timeout: int | None = None
    enable_stack_trace: bool = True
    description: str = ""
    parallel_safe: bool = True  # Whether this process can run in parallel with others
    output_filter: Callable[[str], bool] | None = None  # Filter function for output lines
```

### Process Output Handler

Create a dedicated output handler component:

```python
class ProcessOutputHandler:
    """Handles capturing and displaying process output"""
    
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        
    def handle_output_line(self, line: str, process_name: str) -> None:
        """Process and display a single line of output"""
        # Apply filtering based on verbosity
        if self.verbose or self._should_always_display(line):
            print(f"[{process_name}] {line}")
            
    def _should_always_display(self, line: str) -> bool:
        """Determine if a line should always be displayed regardless of verbosity"""
        return any(marker in line for marker in [
            "FAILED", "ERROR", "Crash", "Running test:", "Test passed", "Test FAILED"
        ])
```

### Process Execution Modes

Support different execution modes:

1. **Parallel Mode**: Run all processes simultaneously with interleaved output
2. **Sequential Mode**: Run processes one after another with full output capture
3. **Group Mode**: Run certain groups in parallel, but groups sequentially

This architecture will provide a more maintainable and extensible test execution system while preserving all existing functionality.
