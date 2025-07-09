#!/bin/bash
set -e

echo "Starting bash test with monitoring..."
echo "PID: $$"

# Function to handle debugging when timeout occurs
debug_hang() {
    echo "=== HANG DETECTED ==="
    echo "Current time: $(date)"
    echo "Process tree:"
    pstree -p $$ 2>/dev/null || echo "pstree not available"
    
    echo "=== PROCESS STATUS ==="
    ps aux | grep -E "(test|uv|python)" | grep -v grep || true
    
    echo "=== MEMORY USAGE ==="
    free -h 2>/dev/null || echo "free command not available"
    
    echo "=== DISK USAGE ==="
    df -h 2>/dev/null || echo "df command not available"
    
    echo "=== ATTEMPTING GDB ATTACH ==="
    
    # Find the main test.py process
    TEST_PID=$(pgrep -f "test.py" | head -1)
    if [ -n "$TEST_PID" ]; then
        echo "Found test.py process: $TEST_PID"
        echo "=== GDB STACK TRACE ==="
        gdb -batch -ex "attach $TEST_PID" -ex "thread apply all bt" -ex "detach" -ex "quit" 2>/dev/null || echo "GDB attach failed"
    else
        echo "No test.py process found"
    fi
    
    # Find uv processes
    UV_PIDS=$(pgrep -f "uv run")
    for pid in $UV_PIDS; do
        echo "=== GDB STACK TRACE FOR UV PROCESS $pid ==="
        gdb -batch -ex "attach $pid" -ex "thread apply all bt" -ex "detach" -ex "quit" 2>/dev/null || echo "GDB attach failed for $pid"
    done
    
    echo "=== KILLING ALL TEST PROCESSES ==="
    pkill -f "test.py" || true
    pkill -f "uv run" || true
    
    exit 1
}

# Run the test with timeout (8 minutes)
echo "Running: timeout 480s bash test"
if timeout 480s bash test; then
    echo "Test completed successfully"
else
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo "Test timed out after 8 minutes"
        debug_hang
    else
        echo "Test failed with exit code: $EXIT_CODE"
        exit $EXIT_CODE
    fi
fi
