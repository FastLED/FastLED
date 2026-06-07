# `fl::string` architecture

Two layers. No template wrappers.

| Type | Role | What it adds |
|---|---|---|
| `fl::basic_string` | **Concrete public class.** Holds *all* string logic (write, append, find, replace, resize, hashing, …), compiled exactly once. Directly constructible from `(char*, fl::size)` or `fl::span<char>`. | Storage state (length + variant<heap, literal, view>) + offset-based pointer to a caller-provided inline buffer. |
| `fl::string` | Default convenience wrapper. Inherits `basic_string` and co-locates a `FASTLED_STR_INLINED_SIZE`-byte inline buffer + the inheritance-passed buffer pointer. Adds composite-type formatters (CRGB, vec2, span, vector, optional, …). | `char mInlineBuffer[64]` + ~30 `append(T)` overloads for FastLED-side composite types. |
| `fl::sstream` | Stream-style facade around an embedded `fl::string`. `operator<<` overloads forward to `mStr.append(...)`. | Sugar — no new storage. |

The historical `fl::StrN<N>` template was removed when `basic_string` became publicly constructible. Callers who previously wrote `fl::StrN<32>` now write `fl::string` (which carries a 64-byte inline buffer + heap overflow — the 32-byte savings turned out not to matter anywhere in the codebase, including the OTA module's 5 fields). Callers who genuinely need a non-default-sized buffer construct `fl::basic_string` directly with `(char*, size)` or `fl::span<char>`.

## Using `basic_string` directly

```cpp
// Ephemeral, caller-owned buffer
char buf[256];
fl::basic_string s(fl::span<char>(buf, 256));
s.append("hello, ");
s.append(42);
// Caller owns buf for as long as s is used.

// Equivalent (pre-#2961 form, still supported)
fl::basic_string s2(buf, 256);
```

**Lifetime contract**: the buffer must outlive the `basic_string`, and the `basic_string` must not be trivially relocated (bitwise-copied to a different address). `basic_string` stores the buffer pointer as an offset from `this` (`mInlineOffset`); relocation invalidates the offset. For member-stored use, prefer `fl::string` (which co-locates the buffer inside itself).

## How the formatting layer trampolines

```text
caller writes:           fl::sstream() << vec2<float>{1.0f, 2.0f}
                          │
                          ▼
sstream::operator<<(vec2<T>):     mStr.append("(");
  (template, in strstream.h)       mStr.append(v.x);   // float overload on fl::string
                                    mStr.append(",");
                                    mStr.append(v.y);
                                    mStr.append(")");
                          │
                          ▼
fl::string::append(float):        basic_string::append(val) — non-template
  (concrete, in basic_string)      │
                                    ▼
basic_string::write(const char*, size):  raw bytes into the inline-or-heap buffer
  (concrete, exactly one copy in the binary)
```

Template instantiations of the structure-walking layer (`sstream::operator<<<T>` for composite T, `string::append<T>` for vec/span/optional/etc.) are short loops that, being defined in the header as implicit-`inline`, are expected to fold under COMDAT at link time. If a future bloat audit shows those instantiations as a top contributor, the next refactor is to push their *bodies* down into non-template `basic_string` helpers with a function-pointer-shaped element formatter.

## Where the actual binary duplication is

Per [#2886](https://github.com/FastLED/FastLED/issues/2886#issuecomment-4643081327) row #24, the largest single string-related symbol on ESP32-S3 Blink is:

```
fl::basic_string::write(const char*, fl::size)    1,151 B (3 overloads)
```

That's `basic_string::write` itself, **not** a template instantiation. The 1.1 KB lives in `src/fl/stl/basic_string.cpp.hpp:193` and comes from the materialize-on-write logic (variant dispatch between inline / heap / literal / view storage modes — see [`basic_string.h::mStorage`](../../src/fl/stl/basic_string.h)). It's already type-erased; the size reflects genuine algorithmic complexity, not duplicated code.

## Storage modes (`basic_string` variant)

The base class supports four storage modes via an `fl::variant`:

| Mode | When | Where the bytes live |
|---|---|---|
| Inline | `mLength + 1 <= mInlineCapacity` | The caller-provided buffer (via `mInlineOffset` from `this`) |
| Heap (`StringHolder`) | `mLength + 1 > mInlineCapacity` | `fl::shared_ptr<StringHolder>` |
| `ConstLiteral` | `setLiteral("...")` | Caller's `.rodata` |
| `ConstView` | `setView(ptr, len)` | Caller's memory |

`is_literal()` / `is_view()` / `is_owning()` queries let callers reason about COW + lifetime. `materialize()` flips a non-owning storage into an owning one before any mutation.

## Reading order

- [`fl/stl/basic_string.h`](../../src/fl/stl/basic_string.h) — base class declaration (the bulk of the API + public constructors)
- [`fl/stl/basic_string.cpp.hpp`](../../src/fl/stl/basic_string.cpp.hpp) — base class impl (the 1.1 KB lives here)
- [`fl/stl/string.h`](../../src/fl/stl/string.h) — `string` wrapper + per-T `append` overloads
- [`fl/stl/strstream.h`](../../src/fl/stl/strstream.h) — `sstream` + per-T `operator<<` overloads
- [`fl/stl/detail/string_holder.h`](../../src/fl/stl/detail/string_holder.h) — heap-backed `StringHolder`
