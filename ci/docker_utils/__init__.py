"""Docker helpers for FastLED — AVR8JS emulator only.

The per-platform compile Docker images (`niteris/fastled-compiler-*`), per-platform
QEMU simulator images (`niteris/fastled-simulator-*`), and the host unit-tests
image (`fastled-unit-tests`) were all retired. Only the AVR8JS emulator wrapper
remains, and it is driven directly by `ci.docker_utils.avr8js_docker` — this
package exists to house that module + its Dockerfile.
"""

__all__: list[str] = []
