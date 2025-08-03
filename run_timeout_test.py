#!/usr/bin/env python3
import subprocess
import sys

print("Running: timeout 60s bash test --examples --verbose | grep BLINK")
print("=" * 60)

try:
    # Run the timeout command
    result = subprocess.run(
        "timeout 60s bash test --examples --verbose | grep BLINK",
        shell=True,
        capture_output=True,
        text=True,
        timeout=65  # Python timeout slightly longer than GNU timeout
    )
    
    print(f"Exit code: {result.returncode}")
    
    if result.returncode == 0:
        print("✅ SUCCESS: Found BLINK output within 60 seconds!")
        lines = result.stdout.strip().split('\n')
        lines = [line for line in lines if line.strip()]
        print(f"Found {len(lines)} lines:")
        for i, line in enumerate(lines, 1):
            print(f"  {i}. {line}")
    elif result.returncode == 124:
        print("⏰ TIMEOUT: Command was killed after 60 seconds")
    else:
        print(f"❌ FAILED: Exit code {result.returncode}")
        if result.stderr.strip():
            print(f"Error: {result.stderr[:200]}...")
    
    if result.stdout.strip():
        print(f"\nOutput captured before timeout/completion:")
        print(result.stdout[:500] + ("..." if len(result.stdout) > 500 else ""))

except subprocess.TimeoutExpired:
    print("⏰ Python timeout expired (command took longer than 65 seconds)")
except Exception as e:
    print(f"❌ Error: {e}")
