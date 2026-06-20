#pragma once

// fl::Fled - value-type handle to a parsed .fled v1 container.
// PIMPL; cheap to copy. Default-constructed instances are in a null state
// and evaluate to false. See FLED_FORMAT.md for the on-disk format.
//
// PR1 surface: factories, raw json()/blob() accessors, version/sectionCount,
// and the null-state contract. Typed accessors (video/screenMap/channels)
// arrive in PR2.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {

class FileSystem;
class json;
class FledImpl;

class Fled {
  public:
    // Load from filesystem. Returns a null Fled on any failure (open fail,
    // truncated, bad magic/version, oversized json_length, parse error).
    static Fled load(FileSystem &fs, const char *path) FL_NO_EXCEPT;

    // Zero-copy load from a static byte span. Span must outlive the Fled
    // and any of its copies (this constructor does NOT copy the bytes).
    static Fled loadFromStatic(fl::span<const fl::u8> bytes) FL_NO_EXCEPT;

    // Move-load from a caller-owned vector. The vector's storage is moved
    // into the impl; no extra copy.
    static Fled loadFromVector(fl::vector<fl::u8> &&bytes) FL_NO_EXCEPT;

    // Default ctor produces a null Fled (operator bool == false).
    Fled() FL_NO_EXCEPT;

    // True iff load succeeded and produced a valid v1 envelope.
    explicit operator bool() const FL_NO_EXCEPT;

    // Parsed JSON envelope. For null Fled, returns a reference to a static
    // empty json (safe to chain into).
    const fl::json &json() const FL_NO_EXCEPT;

    // Returns a shared pointer aliased to the underlying byte storage for
    // the named section. PR1 recognizes "frame_payload" (alias "payload"),
    // which maps to the raw bytes after the JSON envelope. Any other name
    // returns nullptr and writes 0 to *outLen (if non-null).
    //
    // Lifetime: the returned shared_ptr extends FledImpl's lifetime, but
    // does NOT extend the lifetime of bytes loaded via loadFromStatic -
    // those still depend on the caller's span outliving every use.
    fl::shared_ptr<const fl::u8> blob(const char *sectionName,
                                      fl::size *outLen) const FL_NO_EXCEPT;

    // Header version byte. Returns 0 for null Fled, 1 for valid v1.
    fl::u8 version() const FL_NO_EXCEPT;

    // Number of top-level keys in the parsed JSON envelope. 0 for null.
    fl::size sectionCount() const FL_NO_EXCEPT;

  private:
    explicit Fled(fl::shared_ptr<FledImpl> impl) FL_NO_EXCEPT;
    fl::shared_ptr<FledImpl> mImpl;
};

} // namespace fl
