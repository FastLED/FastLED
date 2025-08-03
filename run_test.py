#!/usr/bin/env python3

# Quick test to verify bash test --examples Blink | grep BLINK works

import subprocess
import sys

def main():
    print("🔍 Testing: bash test --examples Blink | grep BLINK")
    print("=" * 50)
    
    try:
        # First run the full command to see all output
        print("Running full test command first...")
        full_result = subprocess.run(
            ["bash", "test", "--examples", "Blink"],
            capture_output=True,
            text=True,
            timeout=600
        )
        
        print(f"Full command exit code: {full_result.returncode}")
        
        if full_result.returncode != 0:
            print("❌ Full command failed!")
            print(f"Stderr: {full_result.stderr[:300]}...")
            return False
        
        # Now test the grep version
        print("\nTesting the grep command...")
        grep_result = subprocess.run(
            "bash test --examples Blink | grep BLINK",
            shell=True,
            capture_output=True,
            text=True,
            timeout=600
        )
        
        print(f"Grep command exit code: {grep_result.returncode}")
        
        if grep_result.returncode == 0:
            lines = grep_result.stdout.strip().split('\n')
            lines = [line for line in lines if line.strip()]
            
            print(f"✅ SUCCESS! Found {len(lines)} lines with 'BLINK':")
            for i, line in enumerate(lines, 1):
                print(f"  {i}. {line}")
            return True
        else:
            print(f"❌ FAILED! grep command returned {grep_result.returncode}")
            print("No 'BLINK' found in output")
            
            # Show some context
            blink_count = full_result.stdout.count('BLINK')
            print(f"However, full output contains 'BLINK' {blink_count} times")
            
            if blink_count > 0:
                print("Sample lines containing BLINK:")
                for line in full_result.stdout.split('\n'):
                    if 'BLINK' in line:
                        print(f"  {line.strip()}")
                        
            return False
    
    except subprocess.TimeoutExpired:
        print("❌ Command timed out after 10 minutes")
        return False
    except Exception as e:
        print(f"❌ Error: {e}")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
