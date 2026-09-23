#pragma once

#include "fl/channels/options.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

class CLEDController;

enum class PowerCodecKind : u8 { Byte, Native7, Native5, FiveBit, Unknown };

struct PowerCodecPolicy {
    PowerCodecKind kind = PowerCodecKind::Unknown;
    u8 min_field = 31;
};

/// Identify the channel's final current/quantization choice without adding
/// a virtual slot or storage to every ordinary CLEDController.
PowerCodecPolicy powerChannelCodecPolicy(const CLEDController& controller)
    FL_NO_EXCEPT;

/// Result of the shared frame-budget prepass on large-memory managed output.
/// `flux_q16` applies to every channel's linear emitter light. Legacy byte
/// encoders quantize the same scalar downward, except an unrestricted plan
/// preserves the exact requested byte to avoid a second rounding loss.
struct FramePowerPlan {
    u32 flux_q16 = 65536;
    u8 legacy_brightness = 255;
    u32 modeled_mW = 0;
    u32 mcu_mW = 0;
    bool infeasible = false;
    bool limited = false;
};

#if FL_COLOR_PIPELINE_SHARED
struct FramePowerDispatch {
    FramePowerPlan (*calculate)(u8 requested_brightness, u32 budget_mW);
    u8 (*showBrightness)(u8 requested_brightness, u32 budget_mW);
    u8 (*showColorBrightness)(u8 requested_brightness, u32 budget_mW);
    void (*endFrame)();
};

/// Referenced only by the built-in power-limit setter, so an ordinary show()
/// does not keep the managed power solver in an unbound sketch.
const FramePowerDispatch* framePowerDispatch() FL_NO_EXCEPT;

/// Solve a frame's maximum shared scalar without advancing dither state.
/// The caller must keep source data and profile bindings stable until encode
/// completes. A below-idle budget returns zero flux and `infeasible=true`.
FramePowerPlan calculateFramePowerPlan(u8 requested_brightness,
                                      u32 budget_mW) FL_NO_EXCEPT;

/// The fixed MCU term included in FramePowerPlan::modeled_mW.
u32 framePowerMCUBaselineMilliwatts() FL_NO_EXCEPT;
#endif

}  // namespace fl
