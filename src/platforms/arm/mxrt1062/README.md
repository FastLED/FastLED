# FastLED Platform: Teensy 4.x (i.MX RT1062)

Teensy 4.0/4.1 (IMXRT1062) support.

## Files (quick pass)
- `fastled_arm_mxrt1062.h`: Aggregator; includes pin/SPI/clockless and helpers.
- `fastpin_arm_mxrt1062.h`: Pin helpers.
- `fastspi_arm_mxrt1062.h`: SPI backend.
- `led_sysdefs_arm_mxrt1062.h`: System defines for RT1062.

The clockless, block-clockless and OctoWS2811 controllers live in
[`../teensy/teensy4_common/`](../teensy/teensy4_common/README.md).

## Multi-Lane SPI Support

For detailed information about Teensy 4.x LPSPI dual/quad-mode support:
- **Platform Documentation**: `src/platforms/arm/teensy/teensy4_common/README.md` - Complete LPSPI feature overview
- **Implementation Guide**: `LP_SPI.md` - Technical details for quad-mode pin configuration

## Notes
- Clockless output timing: see [teensy4_common/README.md](../teensy/teensy4_common/README.md) (ObjectFLED DMA engine).
- OctoWS2811 and SmartMatrix can offload large parallel outputs; ensure pin mappings and DMA settings match board wiring.
