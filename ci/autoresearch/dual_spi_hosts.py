"""Check both explicit ESP32 SPI hosts and registration orders with one jumper.

Flash AutoResearch first, then run from the repository root:

    uv run python ci/autoresearch/dual_spi_hosts.py --port /dev/ttyACM0
"""

from __future__ import annotations

import argparse
import asyncio

from running_process import RunningProcess

from ci.rpc_client import RpcClient, RpcError, RpcTimeoutError
from ci.util.serial_interface import create_serial_interface


async def run_case(port: str, active_host: int, dummy_first: bool) -> bool:
    try:
        async with RpcClient(
            port, serial_interface=create_serial_interface(port)
        ) as client:
            response = await client.send(
                "dualSpiHostLoopback",
                {"activeHost": active_host, "dummyFirst": dummy_first},
                timeout=20,
            )
    except (RpcError, RpcTimeoutError) as exc:
        print(f"HOST{active_host} active, dummy first={dummy_first}: FAIL {exc}")
        return False
    result = response.data
    passed = isinstance(result, dict) and result.get("success") is True
    print(
        f"HOST{active_host} active, dummy first={dummy_first}: "
        f"{'PASS' if passed else 'FAIL'} {result}",
        flush=True,
    )
    return passed


async def reset_board(environment: str, port: str) -> None:
    command = ["fbuild", "reset", "-e", environment, "-p", port]
    for attempt in range(12):
        result = RunningProcess.run(
            command,
            check=False,
            timeout=15,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        if result.returncode == 0:
            await asyncio.sleep(3)
            return
        output = (result.stderr or "").strip() or (result.stdout or "").strip()
        if attempt == 11:
            raise RuntimeError(output or f"fbuild reset failed ({result.returncode})")
        await asyncio.sleep(0.5)


async def run(port: str, environment: str) -> int:
    passed = True
    for active_host in (2, 3):
        for dummy_first in (True, False):
            await reset_board(environment, port)
            if not await run_case(port, active_host, dummy_first):
                passed = False
    return 0 if passed else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="board serial port")
    parser.add_argument("--environment", default="esp32s3", help="fbuild environment")
    args = parser.parse_args()
    return asyncio.run(run(args.port, args.environment))


if __name__ == "__main__":
    raise SystemExit(main())
