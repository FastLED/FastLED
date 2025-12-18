# AVR8JS Docker Emulator for FastLED Testing

This directory contains a Docker-based AVR8JS emulator setup for testing Arduino firmware in CI/CD pipelines.

## Supported Platforms

### ✅ Supported
- **Arduino Uno (ATmega328p)** - Full USART hardware support
- Other ATmega-based boards with hardware USART

### ❌ Not Supported
- **ATtiny85** - See limitation details below

## ATtiny85 Limitation

**ATtiny85 cannot be tested with AVR8JS** due to a fundamental hardware limitation:

### Technical Reason
1. ATtiny85 has **no hardware UART/USART peripheral**
2. Arduino framework uses **TinySoftwareSerial** for serial communication
3. TinySoftwareSerial requires the **Analog Comparator peripheral** (`ANALOG_COMP_vect` interrupt)
4. avr8js emulator **does not support Analog Comparator** for ATtiny85
5. Result: Serial output cannot be captured, tests cannot validate

### Investigation
See `FINDINGS.md` in the project root for complete root cause analysis.

### Alternative Testing
- **Compilation tests**: ATtiny85 is still tested in compilation workflows (confirms code correctness)
- **Physical hardware**: Consider hardware-in-the-loop testing for ATtiny85 validation

## Architecture

```
Test.ino (Arduino sketch)
  ↓ Compile with PlatformIO
firmware.hex
  ↓ Docker volume mount
AVR8JS Emulator (Node.js/TypeScript)
  ↓ Capture serial output
Test validation
```

## Files

- `Dockerfile.avr8js` - Docker image definition
- `main.ts` - CLI entry point and test validation
- `execute.ts` - AVR8JS runner with USART capture
- `avr8js_docker.py` - Python wrapper for CI integration
- `package.json` - Node.js dependencies

## Usage

### From Python (CI)
```python
from ci.docker_utils.avr8js_docker import DockerAVR8jsRunner

runner = DockerAVR8jsRunner()
exit_code = runner.run(
    elf_path=Path("firmware.elf"),
    mcu="atmega328p",
    frequency=16000000,
    timeout=30
)
```

### From Command Line
```bash
uv run python ci/docker_utils/avr8js_docker.py firmware.elf 30
```

### Direct Docker
```bash
docker run --rm -v /path/to/firmware:/firmware:ro \
  niteris/fastled-avr8js:latest /firmware/firmware.hex 30
```

## Adding New Platforms

To add a new platform, ensure:
1. **Hardware USART peripheral** is present
2. **avr8js supports the MCU** (check https://github.com/wokwi/avr8js)
3. Update `avr8js_docker_template.yml` with MCU mapping
4. Test locally before adding to CI

## References

- avr8js: https://github.com/wokwi/avr8js
- Wokwi Docs: https://docs.wokwi.com
- ATtiny85 investigation: `FINDINGS.md`
