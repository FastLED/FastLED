# `fl::string` architecture

Three layers. The top two are thin template wrappers; only the
bottom one carries non-trivial code.

| Type | Role | What it adds |
|---|---|---|
| `fl::basic_string` | **Concrete public class.** Holds *all* string logic (write, append, find, replace, resize, hashing, …), compiled exactly once. Directly constructible from `(char*, fl::size)` or `fl::span<char>`. | Storage state (length + variant<heap, literal, view>) + offset-based pointer to a caller-provided inline buffer. |
| `fl::string_n<N>` | **Templated inline-buffer storage policy.** Holds `char mInlineBuffer[N]` and passes it to `basic_string(mInlineBuffer, N)`. Per-N instantiations are thin constructor stubs — the actual write/append/find/replace logic stays in `basic_string` and is shared. | An inline buffer of exactly `N` bytes + the standard set of `string_n<N>(...)` constructors that hand the buffer to the base. |
| `fl::string` | Default convenience wrapper. Inherits `string_n<FASTLED_STR_INLINED_SIZE>`. Adds composite-type formatters (CRGB, vec2, span, vector, optional, …), the `substring()` / `trim()` family (returning `string` so callers chain naturally), the static factory + comparison methods. | ~30 `append(T)` overloads for FastLED-side composite types, plus the per-string-typed methods (`substring`, comparisons, `operator+=`, factories). |
| `fl::sstream` | Stream-style facade around an embedded `fl::string`. `operator<<` overloads forward to `mStr.append(...)`. | Sugar — no new storage. |

Convenience aliases over `string_n<N>`:

- **`fl::string_small`** — `string_n<32>`. Use on constrained MCUs where the +32-byte default inline buffer is wasteful.
- **`fl::string`** — `string_n<FASTLED_STR_INLINED_SIZE>` (default 64) extended with composite formatters. The everywhere-default.
- **`fl::string_large`** — `string_n<256>`. Use on text-heavy paths where the default's 64-byte cap causes frequent heap promotion.

All three are layout-compatible: copy/move/assign between sizes works because the underlying `basic_string` storage policy is uniform. Choosing a size is purely an "expected inline length" tuning knob — exceeding it triggers heap-backed `StringHolder` promotion in either case.

## Why the template isn't bloat

Each `string_n<N>` instantiation only emits constructor stubs — every one is essentially `: basic_string(mInlineBuffer, N) {}` plus a one-line body that calls a non-template helper on the base (`copy()`, `setLiteral()`, `setSharedHolder()`, etc.). The non-trivial bytes (`write`, `append(i32)`, `find`, `replace`, `resize`, `materialize`) all live in `basic_string` and are compiled exactly once into the FastLED library, then shared by every `N`. The compiler and linker fold identical per-N stubs under COMDAT, so e.g. `string_n<64>` (used everywhere via `fl::string`) is a single set of constructor instantiations no matter how many TUs reference it.

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
