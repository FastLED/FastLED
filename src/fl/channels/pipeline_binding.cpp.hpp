// ok no header - implementation for fl/channels/pipeline_binding.h

#include "fl/channels/pipeline_binding.h"

#include "fl/channels/color_managed_source.h"
#include "fl/stl/new.h"

namespace fl {

bool buildPipelineForBinding(const ColorProfileBinding& binding,
                             StreamingPipelineQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    const EmitterProfile* profile = binding.profile();
    if (profile == nullptr) {
        // Not an error: a channel with no profile bound stays on the legacy
        // path, which is what `active()` being false means.
        return false;
    }
    return buildStreamingPipelineQ16(binding.mSource, *profile, binding.mGamut,
                                     out);
}

namespace {

/// Builds the managed iterator in the caller's storage.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it.
PixelIterator* makeColorPipelineIterator(
    void* source_storage, void* iterator_storage,
    PixelController<RGB, 1, 0xFFFFFFFF>& controller, EOrder order,
    const StreamingPipelineQ16& pipeline, const Rgbw& rgbw,
    Rgbww rgbww) FL_NO_EXCEPT {
    ColorManagedPixelSource* source =
        new (source_storage) ColorManagedPixelSource(controller, order, pipeline);
    return new (iterator_storage) PixelIterator(source, rgbw, rgbww);
}

void destroyColorPipelineIterator(void* source_storage,
                                  void* iterator_storage) FL_NO_EXCEPT {
    static_cast<PixelIterator*>(iterator_storage)->~PixelIterator();
    static_cast<ColorManagedPixelSource*>(source_storage)
        ->~ColorManagedPixelSource();
}

}  // namespace

ColorPipelineHooks& colorPipelineHooks() FL_NO_EXCEPT {
    // Not a function-local static with a non-trivial constructor: this is a
    // zero-initialized aggregate, so there is no guard variable and no
    // Teensy 3.x `__cxa_guard` conflict.
    static ColorPipelineHooks hooks = {nullptr, nullptr, nullptr};
    return hooks;
}

void installColorPipelineHooks() FL_NO_EXCEPT {
    ColorPipelineHooks& hooks = colorPipelineHooks();
    hooks.build = &buildPipelineForBinding;
    hooks.makeIterator = &makeColorPipelineIterator;
    hooks.destroyIterator = &destroyColorPipelineIterator;
}

}  // namespace fl
