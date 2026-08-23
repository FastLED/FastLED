#pragma once

/// @file fl/fs/backend.h
/// @brief The interface a storage backend implements.
///
/// This is the *platform* half of the filesystem subsystem. Sketch authors
/// want `fl/fs/fs.h`; this header is for whoever is teaching FastLED to read
/// from a new medium.
///
/// Splitting it out is the point: previously `FsImpl` shared a header with
/// `FileSystem`, so the two audiences — people opening files and people
/// implementing storage — read the same file for unrelated reasons
/// (FastLED #4003, #4008).
///
/// To add a backend: subclass `FsImpl`, expose a free factory naming the
/// *medium* rather than the driver (`getSdFs`, `getEmbeddedFs` — never
/// `getLittleFs`, see #4007), and hand it to `FileSystem::begin()`.

#include "fl/fs/file_handle.h" // IWYU pragma: export
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

FASTLED_SHARED_PTR(FsImpl);

/// A mounted storage medium. Backends subclass this; sketches never see it
/// except as the opaque `FsImplPtr` a factory hands to `FileSystem::begin()`.
class FsImpl {
  public:
    struct Visitor {
        virtual ~Visitor() FL_NO_EXCEPT {}
        virtual void accept(const char *path) = 0;
    };

    FsImpl() FL_NO_EXCEPT = default;
    virtual ~FsImpl() FL_NO_EXCEPT {}

    /// Mount. False if the medium is absent, unformatted, or unreadable.
    virtual bool begin() = 0;

    /// Unmount. Must be safe to call on a backend that never mounted.
    virtual void end() = 0;

    /// Null handle if the file does not exist or cannot be opened.
    virtual filebuf_ptr openRead(const char *path) = 0;

    /// Optional directory listing. Backends that cannot enumerate return
    /// false rather than pretending the medium is empty.
    virtual bool ls(Visitor &visitor) {
        (void)visitor;
        return false;
    }
};

} // namespace fl
