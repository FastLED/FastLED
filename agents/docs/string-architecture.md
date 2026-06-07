# `fl::string` architecture — the trampoline pattern

Public names (in `fl/stl/`):

| Type | Role | What it adds over the base |
|---|---|---|
| `fl::string_base` (alias for `fl::basic_string`) | **Concrete, type-erased base.** Holds *all* string logic (write, append, find, replace, resize, hashing, …). Compiled exactly once. | n/a — every storage policy delegates here. |
| `fl::stringN<N>` (alias for `fl::StrN<N>`) | Inline-buffer storage policy. Holds `char buf[N]` and passes `(buf, N)` to the base ctor. | A fixed-size inline buffer. Empty `mInlineBuffer[N]={0}` + thin constructors/assignment. |
| `fl::string` | The default-everywhere class. `stringN<64>` (i.e. `StrN<FASTLED_STR_INLINED_SIZE>`) plus a long list of `append(T)` / `operator+` overloads for FastLED-side types (CRGB, vec2, span, vector, optional, …). | Per-T composite-formatter overloads. Bodies are short loops that call `append(elem)` on each member, where `append(elem)` itself dispatches to `string_base`. |
| `fl::sstream` | Stream-style facade around an embedded `fl::string`. `operator<<` overloads forward to `mStr.append(...)`. | Sugar — no new storage. |

## How the trampoline works

```
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

The **template layer** (`sstream::operator<<<T>`, `string::append<T>` for composites) walks T's structure. The **leaf calls** all land on non-template `basic_string` methods (`write(const char*, size)`, `append(float)`, `append(i32)`, …) which are compiled exactly once. Template instantiations of the structure-walking layer are short loops that — being defined in the header as implicit-`inline` — should fold under COMDAT at link time.

## Where the actual binary duplication is

Per [#2886](https://github.com/FastLED/FastLED/issues/2886#issuecomment-4643081327) row #24, the largest single string-related symbol on ESP32-S3 Blink is:

```
fl::basic_string::write(const char*, fl::size)    1,151 B (3 overloads)
```

That's `basic_string::write` itself, **not** a template instantiation. The 1.1 KB lives in `src/fl/stl/basic_string.cpp.hpp:193` and comes from the materialize-on-write logic (variant dispatch between inline / heap / literal / view storage modes — see [`basic_string.h:466`](../../src/fl/stl/basic_string.h)). It's already type-erased; the size reflects genuine algorithmic complexity, not duplicated code.

Template-instantiation duplication of `string::append<T>(...)` / `sstream::operator<<<T>(T)` does happen for composite types (vec2, vector, span, …) but those instantiations are individually tiny (each is a 3-5 line loop that all reduces to the same `basic_string::write` calls), and the linker generally folds identical instantiations into one COMDAT group. If a future bloat audit shows the composite formatters as a top contributor, the fix is to push their *bodies* down into non-template `basic_string` helpers that take a function-pointer-shaped element formatter — not to reshape the storage classes.

## Why the names are aliases, not renames

The codebase already uses `basic_string` / `StrN<N>` / `string` extensively (~hundreds of references). Adding the `string_base` and `stringN<N>` aliases gives newer code a clearer vocabulary without churning every existing site. Hard renames can come later as a follow-up if the vocabulary stabilises.

## Storage modes (`basic_string` variant)

The base class supports four storage modes via an `fl::variant`:

| Mode | When | Where the bytes live |
|---|---|---|
| Inline | `mLength + 1 <= mInlineCapacity` | The wrapper's `mInlineBuffer[N]` (via `mInlineOffset` from `this`) |
| Heap (`StringHolder`) | `mLength + 1 > mInlineCapacity` | `fl::shared_ptr<StringHolder>` |
| `ConstLiteral` | `setLiteral("...")` | Caller's `.rodata` |
| `ConstView` | `setView(ptr, len)` | Caller's memory |

`is_literal()` / `is_view()` / `is_owning()` queries let callers reason about COW + lifetime. `materialize()` flips a non-owning storage into an owning one before any mutation.

## Reading order

- [`fl/stl/basic_string.h`](../../src/fl/stl/basic_string.h) — base class declaration (the bulk of the API)
- [`fl/stl/basic_string.cpp.hpp`](../../src/fl/stl/basic_string.cpp.hpp) — base class impl (the 1.1 KB lives here)
- [`fl/stl/string.h`](../../src/fl/stl/string.h) — `StrN<N>` + `string` + per-T `append` overloads
- [`fl/stl/strstream.h`](../../src/fl/stl/strstream.h) — `sstream` + per-T `operator<<` overloads
- [`fl/stl/detail/string_holder.h`](../../src/fl/stl/detail/string_holder.h) — heap-backed `StringHolder`
