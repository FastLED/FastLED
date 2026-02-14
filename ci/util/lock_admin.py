from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
Lock Administration CLI

Command-line tool for managing and diagnosing cache locks.
Useful for manual recovery and troubleshooting.

Usage:
    uv run python -m ci.util.lock_admin --list
    uv run python -m ci.util.lock_admin --clean-stale
    uv run python -m ci.util.lock_admin --force-unlock-all
    uv run python -m ci.util.lock_admin --check <lock-name>
"""

import argparse
import sys
from pathlib import Path

from ci.util.cache_lock import (
    cleanup_stale_locks,
    force_unlock_cache,
    list_active_locks,
)
from ci.util.file_lock_rw import is_process_alive
from ci.util.lock_database import get_lock_database


def list_locks(cache_dir: Path) -> int:
    """List all locks in cache directory with status."""
    locks = list_active_locks(cache_dir)

    # Also show all locks from the database
    db = get_lock_database()
    all_db_locks = db.list_all_locks()

    if not locks and not all_db_locks:
        print("No locks found")
        return 0

    print(f"\n📋 ACTIVE LOCKS")
    print("=" * 80)

    active_count = 0
    stale_count = 0

    # Show locks from cache_dir filtering
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
        print(f"  Lock: {lock.path}")
        print(f"  PID: {pid}")
        print(f"  Operation: {operation}")
        print(f"  Timestamp: {timestamp}")
        print(f"  Hostname: {hostname}")

    # Show any DB locks not already shown
    shown_names = {lock.path for lock in locks}
    for db_lock in all_db_locks:
        lock_name = db_lock["lock_name"]
        if lock_name not in shown_names:
            pid = db_lock["owner_pid"]
            alive = is_process_alive(pid)
            status_icon = "🟢 ACTIVE" if alive else "🔴 STALE"

            if alive:
                active_count += 1
            else:
                stale_count += 1

            print(f"\n{status_icon}")
            print(f"  Lock: {lock_name}")
            print(f"  PID: {pid}")
            print(f"  Operation: {db_lock['operation']}")
            print(f"  Mode: {db_lock['lock_mode']}")
            print(f"  Hostname: {db_lock['hostname']}")

    print("\n" + "=" * 80)
    total = active_count + stale_count
    print(f"Total: {total} locks ({active_count} active, {stale_count} stale)")

    return 0


def clean_stale(cache_dir: Path) -> int:
    """Clean only stale locks (safe operation)."""
    print(f"🧹 Cleaning stale locks...")

    # Clean from database
    db = get_lock_database()
    db_cleaned = db.cleanup_stale_locks()

    # Clean from cache_dir (legacy files)
    cache_cleaned = cleanup_stale_locks(cache_dir)

    total = db_cleaned + cache_cleaned
    if total > 0:
        print(f"✅ Cleaned {total} stale lock(s)")
    else:
        print("✅ No stale locks found")

    return 0


def force_unlock_all(cache_dir: Path) -> int:
    """Force break all locks (destructive operation)."""
    print(f"⚠️  WARNING: Forcing unlock of ALL locks")
    print("This will break locks from active processes!")

    response = input("Are you sure? (yes/no): ")

    if response.lower() != "yes":
        print("Cancelled")
        return 1

    # Force break all in database
    db = get_lock_database()
    db_broken = db.force_break_all()

    # Force break in cache_dir (legacy files)
    cache_broken = force_unlock_cache(cache_dir)

    total = db_broken + cache_broken
    if total > 0:
        print(f"✅ Broke {total} lock(s)")
    else:
        print("✅ No locks found")

    return 0


def check_lock(lock_name: str) -> int:
    """Check status of a specific lock."""
    print(f"\n🔍 LOCK INFORMATION: {lock_name}")
    print("=" * 80)

    db = get_lock_database()
    holders = db.get_lock_info(lock_name)

    if not holders:
        print(f"❌ No lock found with name: {lock_name}")
        return 1

    for holder in holders:
        pid = holder["owner_pid"]
        operation = holder["operation"]
        hostname = holder["hostname"]
        mode = holder["lock_mode"]
        acquired_at = holder["acquired_at"]

        print(f"PID: {pid}")
        print(f"Operation: {operation}")
        print(f"Mode: {mode}")
        print(f"Acquired at: {acquired_at}")
        print(f"Hostname: {hostname}")

        if is_process_alive(pid):
            print(f"Process status: 🟢 ALIVE (PID {pid} is running)")
            print("Lock status: ACTIVE")
        else:
            print(f"Process status: 🔴 DEAD (PID {pid} not found)")
            print("Lock status: STALE")

    print("=" * 80)

    # Offer to break stale lock
    stale_holders = [h for h in holders if not is_process_alive(h["owner_pid"])]
    if stale_holders:
        response = input("\nStale holder(s) found. Break them? (yes/no): ")
        if response.lower() == "yes":
            if db.break_stale_lock(lock_name):
                print("✅ Stale lock(s) broken successfully")
                return 0
            else:
                print("❌ Failed to break stale lock(s)")
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

  # Check specific lock
  uv run python -m ci.util.lock_admin --check "build:libfastled_build"
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
        type=str,
        metavar="LOCK_NAME",
        help="Check status of specific lock (by name)",
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
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
