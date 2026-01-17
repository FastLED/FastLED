from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Generate platformio.ini from board configuration.

Usage:
    python3 -m ci.docker.generate_platformio_ini <board_name> [project_root]
"""

import sys


def main():
    if len(sys.argv) < 2:
        print("Error: board_name argument required", file=sys.stderr)
        print(
            "Usage: python3 -m ci.docker.generate_platformio_ini <board_name> [project_root]",
            file=sys.stderr,
        )
        sys.exit(1)

    board_name = sys.argv[1]
    project_root = sys.argv[2] if len(sys.argv) > 2 else "/fastled"

    try:
        from ci.boards import create_board

        board = create_board(board_name)
        ini_content = board.to_platformio_ini(
            include_platformio_section=True,
            project_root=project_root,
            additional_libs=["FastLED"],
        )

        # Write to platformio.ini
        with open("platformio.ini", "w") as f:
            f.write(ini_content)

        print("Successfully generated platformio.ini")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Error generating platformio.ini: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
