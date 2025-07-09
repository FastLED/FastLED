#!/bin/bash
set -e

echo "=== HANG DEBUGGING SCRIPT ==="
echo "Starting at: $(date)"
echo "PID: $$"

# Create a temporary script to run bash test
cat > /tmp/run_test.sh << 'EOF'
#!/bin/bash
set -x
cd /workspace
bash test
EOF

chmod +x /tmp/run_test.sh

# Function to capture system state
capture_state() {
    local reason="$1"
    echo "=== STATE CAPTURE: $reason ==="
    echo "Time: $(date)"
    echo "Uptime: $(uptime)"
    
    echo "=== PROCESS TREE ==="
    pstree -p $$ 2>/dev/null || echo "pstree not available"
    
    echo "=== ALL PYTHON/UV PROCESSES ==="
    ps aux | grep -E "(python|uv|test)" | grep -v grep || true
    
    echo "=== MEMORY USAGE ==="
    free -h 2>/dev/null || echo "free not available"
    
    echo "=== DISK USAGE ==="
    df -h 2>/dev/null || echo "df not available"
    
    echo "=== OPEN FILES FOR PYTHON PROCESSES ==="
    for pid in $(pgrep -f "python.*test"); do
        echo "Process $pid open files:"
        lsof -p $pid 2>/dev/null | head -20 || echo "lsof not available"
    done
    
    echo "=== NETWORK CONNECTIONS ==="
    netstat -tuln 2>/dev/null | head -10 || echo "netstat not available"
    
    echo "=== SYSTEM LOAD ==="
    cat /proc/loadavg 2>/dev/null || echo "loadavg not available"
}

# Start monitoring in background
(
    sleep 30
    capture_state "30 seconds"
    
    sleep 30
    capture_state "60 seconds"
    
    sleep 60
    capture_state "120 seconds"
    
    sleep 60
    capture_state "180 seconds"
    
    sleep 60
    capture_state "240 seconds"
    
    sleep 60
    capture_state "300 seconds - FINAL"
) &

MONITOR_PID=$!

echo "Starting bash test with timeout..."
timeout 300 /tmp/run_test.sh

EXIT_CODE=$?

# Kill monitor if test completed
kill $MONITOR_PID 2>/dev/null || true

echo "=== TEST COMPLETED ==="
echo "Exit code: $EXIT_CODE"
echo "Time: $(date)"

if [ $EXIT_CODE -eq 124 ]; then
    echo "=== TIMEOUT OCCURRED ==="
    capture_state "TIMEOUT"
    
    # Try to find hanging processes
    echo "=== HANGING PROCESSES ==="
    for pid in $(pgrep -f "python.*test"); do
        echo "Attempting gdb attach to $pid..."
        gdb -batch -ex "attach $pid" -ex "thread apply all bt" -ex "detach" -ex "quit" 2>/dev/null || echo "gdb failed for $pid"
    done
fi

echo "=== CLEANUP ==="
# Kill any remaining test processes
pkill -f "python.*test" 2>/dev/null || true
pkill -f "uv.*test" 2>/dev/null || true

echo "Debug script completed at: $(date)"
