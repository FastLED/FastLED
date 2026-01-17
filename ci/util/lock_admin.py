from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Lock Administration CLI

Command-line tool for managing and diagnosing cache locks.
Useful for manual recovery and troubleshooting.

Usage:
    uv run python -m ci.util.lock_admin --list
    uv run python -m ci.util.lock_admin --clean-stale
    uv run python -m ci.util.lock_admin --force-unlock-all
    uv run python -m ci.util.lock_admin --check <lock-file>
"""

import argparse
import sys
from pathlib import Path

from ci.util.cache_lock import (
    cleanup_stale_locks,
    force_unlock_cache,
    list_active_locks,
)
from ci.util.file_lock_rw import (
    _read_lock_metadata,
    break_stale_lock,
    is_process_alive,
)


def list_locks(cache_dir: Path) -> int:
    """List all locks in cache directory with status."""
    locks = list_active_locks(cache_dir)

    if not locks:
        print("No locks found in cache directory")
        return 0

    print(f"\n📋 ACTIVE LOCKS IN {cache_dir}")
    print("=" * 80)

    active_count = 0
    stale_count = 0

    for lock in locks:
        status_icon = "🔴 STALE" if lock.is_stale else "🟢 ACTIVE"
        pid = lock.pid if lock.pid else "unknown"
        operation = lock.operation
        timestamp = lock.timestamp if lock.timestamp else "unknown"
        hostname = lock.hostname if lock.hostname else "unknown"

        if lock.is_stale:
            stale_count += 1
        else:
            active_count += 1

        print(f"\n{status_icon}")
        print(f"  Path: {lock.path}")
        print(f"  PID: {pid}")
        print(f"  Operation: {operation}")
        print(f"  Timestamp: {timestamp}")
        print(f"  Hostname: {hostname}")

    print("\n" + "=" * 80)
    print(f"Total: {len(locks)} locks ({active_count} active, {stale_count} stale)")

    return 0


def clean_stale(cache_dir: Path) -> int:
    """Clean only stale locks (safe operation)."""
    print(f"🧹 Cleaning stale locks in {cache_dir}...")

    cleaned = cleanup_stale_locks(cache_dir)

    if cleaned > 0:
        print(f"✅ Cleaned {cleaned} stale lock(s)")
    else:
        print("✅ No stale locks found")

    return 0


def force_unlock_all(cache_dir: Path) -> int:
    """Force break all locks (destructive operation)."""
    print(f"⚠️  WARNING: Forcing unlock of ALL locks in {cache_dir}")
    print("This will break locks from active processes!")

    response = input("Are you sure? (yes/no): ")

    if response.lower() != "yes":
        print("Cancelled")
        return 1

    broken = force_unlock_cache(cache_dir)

    if broken > 0:
        print(f"✅ Broke {broken} lock(s)")
    else:
        print("✅ No locks found")

    return 0


def check_lock(lock_file: Path) -> int:
    """Check status of a specific lock file."""
    if not lock_file.exists():
        print(f"❌ Lock file does not exist: {lock_file}")
        return 1

    print(f"\n🔍 LOCK INFORMATION: {lock_file}")
    print("=" * 80)

    # Read metadata
    metadata = _read_lock_metadata(lock_file)

    if metadata:
        pid = metadata.get("pid")
        operation = metadata.get("operation", "unknown")
        timestamp = metadata.get("timestamp", "unknown")
        hostname = metadata.get("hostname", "unknown")

        print(f"PID: {pid}")
        print(f"Operation: {operation}")
        print(f"Timestamp: {timestamp}")
        print(f"Hostname: {hostname}")

        if pid:
            if is_process_alive(pid):
                print(f"Process status: 🟢 ALIVE (PID {pid} is running)")
                print("Lock status: ACTIVE")
            else:
                print(f"Process status: 🔴 DEAD (PID {pid} not found)")
                print("Lock status: STALE")
    else:
        print("⚠️  No metadata found for this lock")
        print("Lock may be from an old version or corrupted")

    print("=" * 80)

    # Offer to break stale lock
    if metadata:
        pid = metadata.get("pid")
        if pid is not None and isinstance(pid, int) and not is_process_alive(pid):
            response = input("\nThis lock is stale. Break it? (yes/no): ")
            if response.lower() == "yes":
                if break_stale_lock(lock_file):
                    print("✅ Lock broken successfully")
                    return 0
                else:
                    print("❌ Failed to break lock")
                    return 1

    return 0


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Lock administration tool for cache management",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # List all locks with status
  uv run python -m ci.util.lock_admin --list

  # Clean only stale locks (safe)
  uv run python -m ci.util.lock_admin --clean-stale

  # Force unlock everything (DANGEROUS!)
  uv run python -m ci.util.lock_admin --force-unlock-all

  # Check specific lock file
  uv run python -m ci.util.lock_admin --check ~/.platformio/global_cache/toolchain-xyz/.download.lock
        """,
    )

    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=Path.home() / ".platformio" / "global_cache",
        help="Cache directory (default: ~/.platformio/global_cache)",
    )

    parser.add_argument(
        "--list",
        action="store_true",
        help="List all locks with their status",
    )

    parser.add_argument(
        "--clean-stale",
        action="store_true",
        help="Remove only stale locks (safe - checks PID)",
    )

    parser.add_argument(
        "--force-unlock-all",
        action="store_true",
        help="Force remove ALL locks (DANGEROUS - no PID check)",
    )

    parser.add_argument(
        "--check",
        type=Path,
        metavar="LOCK_FILE",
        help="Check status of specific lock file",
    )

    args = parser.parse_args()

    # Require at least one action
    if not any([args.list, args.clean_stale, args.force_unlock_all, args.check]):
        parser.print_help()
        return 1

    # Execute requested action
    try:
        if args.list:
            return list_locks(args.cache_dir)

        if args.clean_stale:
            return clean_stale(args.cache_dir)

        if args.force_unlock_all:
            return force_unlock_all(args.cache_dir)

        if args.check:
            return check_lock(args.check)

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n❌ Interrupted by user")
        return 130
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
