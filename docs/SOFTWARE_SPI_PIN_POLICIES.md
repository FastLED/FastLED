# Static pin customization in generic software SPI

This note describes the internal `BitbangPin` contract used by
`GenericSoftwareSPIOutput`. It does not define a new public pin API and does not
apply to runtime SPI engines that use their own GPIO providers.

## Current limitation

Overriding a hardware pin base class's static `hi()` and `lo()` methods is **not
sufficient** to change every data write made by generic software SPI.
Platform `BitbangPin` specializations can match `FastPin<PIN>` itself or, on
some platforms, its hardware base, then write set/clear registers directly.
A custom pin can still select that policy, so its overridden methods may be
bypassed. Whole-port snapshot or streaming paths can also bypass those methods.

This limitation is tracked in [#5122](https://github.com/FastLED/FastLED/issues/5122).
There is currently no in-repository pin relying on this derived-method
customization. General native-method identity metadata and automatic override
detection are deferred until there is a concrete downstream case and a tested
cross-platform contract. This documentation does not claim that detection is
implemented or that the issue is fixed.

## Existing workaround: provide the matching pin policy

An advanced integration that supplies a custom `FastPin` must also supply a
matching `BitbangPin` policy if its GPIO semantics differ from the recognized
hardware base. Declare the full specialization before the policy or its SPI
writer is first instantiated. The pin specialization itself must already be
valid for that platform; this does not permit redefining an existing platform
specialization or make virtual pin numbers work in unrelated channel APIs.
The same explicit policy must be visible in every translation unit that
instantiates that writer; put it in a common integration header included before
SPI use to avoid inconsistent definitions and an ODR violation.

For example, with the integration's compile-time pin constant
`kApplicationPin` and its custom `FastPin` already declared:

```cpp
namespace fl {
template <>
struct BitbangPin<FastPin<kApplicationPin>>
    : BitbangPinDefaults<FastPin<kApplicationPin>> {
    static constexpr bool kAtomicWrite = true;
    static constexpr bool kTwoWriteStream = false;
};
} // namespace fl
```

`BitbangPinDefaults::write()` calls the selected pin's `hi()` or `lo()`.
The flags above ask the generic writer to use per-pin operations rather than
whole-port snapshots or two-write streaming. Apply the policy to each custom
pin whose operations need this treatment, including a custom clock pin.
In the per-pin path, clock transitions call `FastPin<CLOCK_PIN>::hi()` and
`lo()` directly. A custom clock policy prevents the bulk paths from bypassing
those calls and selects its delay implementation.
Here `kAtomicWrite` is a writer-selection property, **not** a guarantee that the
integration's GPIO implementation is atomic or synchronized.

Keep the default conservative latency metadata unless it has been measured for
the custom operations. Inheriting the default policy also uses its generic delay
path rather than a platform policy's specialized timing implementation. Do not
copy native streaming or timing hints merely because the custom pin derives
from the same hardware class. Validate logical edges and unrelated-pin
preservation in tests, then validate electrical polarity and timing on the
actual board before claiming a rate or enabling a hardware optimization.

The relevant implementation is
[`bitbang_pin.h`](../src/platforms/shared/spi_bitbang/bitbang_pin.h), with writer
selection in
[`generic_software_spi.h`](../src/platforms/shared/spi_bitbang/generic_software_spi.h).
