// ok no header - implementation for fl/channels/pipeline_binding.h

#include "fl/channels/pipeline_binding.h"

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

}  // namespace fl
