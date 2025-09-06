# RunningProcessGroup Implementation - COMPLETED ✅

## Investigation Results

### Current Ad-Hoc Structure

The current system manages `RunningProcess` instances through several patterns:

1. **Process Lists** (`ci/util/test_runner.py`):
   - Functions like `get_cpp_test_processes()`, `get_python_test_processes()` return `list[RunningProcess]`
   - Manual assembly: `processes: list[RunningProcess] = []` followed by `processes.append()`
   - Passed to `run_test_processes(processes, parallel=True)`

2. **Sequential Execution** (`test.py`):
   - Manual process creation with `auto_run=False`
   - Complex dependency management using `on_complete` callbacks
   - Example: `python_process.on_complete = start_examples_compilation`

3. **Parallel Execution** (`test_runner.py:_run_processes_parallel()`):
   - Complex monitoring with multiple tracking lists:
     - `active_processes`
     - `failed_processes` 
     - `exit_failed_processes`
   - Manual timeout/stuck detection with dedicated monitoring threads

4. **Global Management** (`running_process_manager.py`):
   - Singleton pattern for process registration/cleanup
   - Each process self-registers for diagnostics

### Key Pain Points
- Manual list management and process assembly
- Scattered execution logic across multiple functions
- Complex callback-based dependency management
- Duplicated monitoring and error handling code
- No clear abstraction for different execution patterns

## Design Decision: RunningProcessGroup

### Class Names
- **Primary**: `RunningProcessGroup`
- **Supporting**: `ProcessExecutionConfig` 
- **Enum**: `ExecutionMode`

### Location
- **File**: `ci/util/running_process_group.py` (new module)
- **Rationale**: Keeps related functionality together, follows existing `ci/util/` pattern

### Architecture Decisions

#### Not Using Singleton Pattern
**Rationale**: Analysis shows multiple distinct process groups with different execution requirements:
- Main test suite (parallel)
- Sequential compilation chains  
- Dependency-based execution
- Different timeout/failure thresholds

A singleton would not provide the needed flexibility.

#### Using Typed Dataclasses
**Decision**: Yes, for configuration objects
**Rationale**: Provides type safety, clear parameter documentation, and default values

### Constructor Signatures

```python
from dataclasses import dataclass
from enum import Enum
from typing import List, Optional, Callable
from ci.util.running_process import RunningProcess

class ExecutionMode(Enum):
    PARALLEL = "parallel"
    SEQUENTIAL = "sequential" 
    SEQUENTIAL_WITH_DEPENDENCIES = "sequential_with_dependencies"

@dataclass
class ProcessExecutionConfig:
    """Configuration for how a process group should execute"""
    execution_mode: ExecutionMode = ExecutionMode.PARALLEL
    timeout_seconds: Optional[int] = None
    max_failures_before_abort: int = 3
    verbose: bool = False
    enable_stuck_detection: bool = True
    stuck_timeout_seconds: int = 300

class RunningProcessGroup:
    def __init__(
        self, 
        processes: List[RunningProcess] = None,
        config: ProcessExecutionConfig = None,
        name: str = "ProcessGroup"
    ):
        self.processes = processes or []
        self.config = config or ProcessExecutionConfig()
        self.name = name
        self._dependencies: Dict[RunningProcess, List[RunningProcess]] = {}
    
    def add_process(self, process: RunningProcess) -> None:
        """Add a process to the group"""
        
    def add_dependency(self, process: RunningProcess, depends_on: RunningProcess) -> None:
        """Add a dependency relationship between processes"""
        
    def run(self) -> List[ProcessTiming]:
        """Execute all processes according to the configuration"""
        
    def add_sequential_chain(self, processes: List[RunningProcess]) -> None:
        """Add processes that must run in sequence"""
```

### Expected Usage Patterns

#### Current Pattern (Replace This):
```python
# Scattered across multiple functions
processes = []
processes.append(create_namespace_check_process(enable_stack_trace))
processes.append(create_unit_test_process(args, enable_stack_trace))
run_test_processes(processes, parallel=True, verbose=args.verbose)
```

#### New Pattern (Parallel Execution):
```python
config = ProcessExecutionConfig(
    execution_mode=ExecutionMode.PARALLEL,
    max_failures_before_abort=3,
    verbose=args.verbose
)
group = RunningProcessGroup(config=config, name="MainTestSuite")
group.add_process(create_namespace_check_process(enable_stack_trace))
group.add_process(create_unit_test_process(args, enable_stack_trace))
timings = group.run()
```

#### New Pattern (Sequential with Dependencies):
```python
config = ProcessExecutionConfig(
    execution_mode=ExecutionMode.SEQUENTIAL_WITH_DEPENDENCIES,
    timeout_seconds=1800
)
group = RunningProcessGroup(config=config, name="SequentialTests")

python_proc = create_python_test_process(False, True)
examples_proc = create_examples_test_process(args, enable_stack_trace)

group.add_process(python_proc)
group.add_process(examples_proc)
group.add_dependency(examples_proc, depends_on=python_proc)

timings = group.run()
```

### Implementation Benefits

1. **Encapsulation**: All process management logic in one place
2. **Configuration**: Clear, typed configuration options
3. **Flexibility**: Supports existing patterns while enabling new ones
4. **Maintainability**: Eliminates code duplication
5. **Type Safety**: Typed dataclasses and clear interfaces
6. **Backward Compatibility**: Can be introduced incrementally

### Migration Strategy

1. Create `RunningProcessGroup` class
2. Migrate `_run_processes_parallel()` logic into the class
3. Update test runner functions to use new class
4. Replace ad-hoc list management patterns
5. Remove duplicated monitoring/error handling code

This design addresses all current pain points while maintaining the existing functionality and following FastLED codebase patterns.

## Implementation Status ✅

**COMPLETED**: The `RunningProcessGroup` class has been fully implemented in `ci/util/running_process_group.py`

### What Was Implemented

1. ✅ **Core Classes**:
   - `ExecutionMode` enum (PARALLEL, SEQUENTIAL, SEQUENTIAL_WITH_DEPENDENCIES)
   - `ProcessExecutionConfig` dataclass with typed configuration options
   - `RunningProcessGroup` main class with all planned methods

2. ✅ **Execution Modes**:
   - **Parallel**: Migrated logic from `_run_processes_parallel()` with full monitoring, stuck detection, and failure thresholds
   - **Sequential**: Simple sequential execution with early failure detection
   - **Dependency-based**: Topological execution respecting process dependencies

3. ✅ **Key Features**:
   - Process management (`add_process`, `add_dependency`, `add_sequential_chain`)
   - Comprehensive error handling and failure reporting
   - Configurable timeout and stuck detection
   - Full timing collection for performance analysis
   - Backward compatible with existing `RunningProcess` and `ProcessTiming` classes

4. ✅ **Testing**: Implementation verified with multiple execution scenarios

### Migration Completed ✅

The `RunningProcessGroup` has been successfully integrated into the FastLED codebase:

**Files Updated:**
- `ci/util/test_runner.py`: `run_test_processes()` now uses `RunningProcessGroup`
- `test.py`: Complex sequential execution logic replaced with dependency-based execution
- Deprecated functions marked with clear deprecation warnings

**Backward Compatibility:**
- All existing test execution paths continue to work unchanged
- Deprecated functions (`_run_processes_parallel`, `_run_process_with_output`) remain available but marked for future removal
- All configuration options and behaviors preserved

**Benefits Realized:**
- ✅ Eliminated complex ad-hoc process management code
- ✅ Centralized execution logic in one maintainable class
- ✅ Improved dependency management for sequential execution
- ✅ Better error handling and failure reporting  
- ✅ Type safety with clear configuration interfaces
- ✅ Comprehensive testing validates migration success
