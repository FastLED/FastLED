#pragma once

// IWYU pragma: private

/// @file coroutine_teensy.hpp
/// @brief Teensy 4.x coroutine infrastructure — declarations and implementations
///
/// Contains ARM Cortex-M7 context switching, cooperative coroutine platform/runtime,
/// task coroutine, and factory functions.
/// This file must only be included from a single aggregate (.cpp.hpp).

#include "platforms/arm/teensy/is_teensy.h"

#ifdef FL_IS_TEENSY_4X

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "platforms/coroutine.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/functional.h"
#include "fl/stl/unique_ptr.h"
#include "fl/int.h"
#include "fl/singleton.h"
#include "fl/warn.h"
#include "fl/arduino.h"
// IWYU pragma: end_keep

// IWYU pragma: begin_keep
#include <stdlib.h>  // malloc, free
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

//=============================================================================
// ARM Cortex-M7 Constants and Stack Initialization
//=============================================================================

/// Context frame: 16 FPU regs + 1 FPSCR + 9 core = 26 words = 104 bytes
static constexpr size_t kContextFrameWords = 26;
static constexpr size_t kContextFrameSize = kContextFrameWords * sizeof(fl::u32);
static constexpr size_t kMinCoroutineStackSize = 1024;
static constexpr size_t kDefaultCoroutineStackSize = 4096;
static constexpr fl::u32 kStackCanaryValue = 0xDEADC0DE;

/// @brief Initialize a coroutine stack so context_switch will "return" into entry_fn
inline void* init_coroutine_stack(void* stack_top, void (*entry_fn)()) {
    uintptr_t sp = reinterpret_cast<uintptr_t>(stack_top);  // ok reinterpret cast
    sp &= ~7u;  // align down to 8 bytes
    sp -= kContextFrameSize;

    fl::u32* frame = reinterpret_cast<fl::u32*>(sp);  // ok reinterpret cast
    for (size_t i = 0; i < kContextFrameWords; ++i) {
        frame[i] = 0;
    }
    // lr/pc slot = entry_fn with Thumb bit set
    frame[25] = reinterpret_cast<fl::u32>(entry_fn) | 1u;  // ok reinterpret cast
    return reinterpret_cast<void*>(sp);  // ok reinterpret cast
}

//=============================================================================
// Coroutine Platform — ARM Cortex-M7 context switching
//=============================================================================

struct TeensyContextState {
    void* saved_sp = nullptr;
    void* stack_base = nullptr;
    size_t stack_size = 0;
};

class CoroutinePlatformTeensy : public ICoroutinePlatform {
public:
    void* createContext(void (*entry_fn)(), size_t stack_size) override;
    void* createRunnerContext() override;
    void destroyContext(void* ctx) override;
    void contextSwitch(void* from_ctx, void* to_ctx) override;
    bool inInterruptContext() const override;
    bool checkStackHealth(void* ctx) const override;
    fl::u32 micros() const override;
};

//=============================================================================
// Coroutine Runtime — pumps cooperative coroutine runner
//=============================================================================

class CoroutineRuntimeTeensy : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) override {
        if (CoroutineContext::isInsideCoroutine()) {
            CoroutineContext::suspend();
        } else {
            CoroutineRunner::instance().run(us);
        }
    }
};

using CoroutineContextTeensy = CoroutineContext;
using CoroutineRunnerTeensy = CoroutineRunner;

//=============================================================================
// Task Coroutine
//=============================================================================

class TaskCoroutineTeensy : public ICoroutineTask {
public:
    static TaskCoroutinePtr create(fl::string name,
                                    TaskFunction function,
                                    size_t stack_size = 4096,
                                    u8 priority = 5);

    ~TaskCoroutineTeensy() override;

    void stop() override;
    bool isRunning() const override;

private:
    TaskCoroutineTeensy() = default;

    fl::string mName;
    fl::unique_ptr<CoroutineContext> mContext;
};

//=============================================================================
// ARM Cortex-M7 Context Switch — naked asm
//=============================================================================

// context_switch(void** old_sp, void* new_sp)
//
// r0 = old_sp (pointer to save location for current SP)
// r1 = new_sp (stack pointer to restore)
//
// Uses MSP/PSP dual stack pointer mechanism:
//   - Runner (SPSEL=0): saves to MSP, loads PSP, sets SPSEL=1
//   - Coroutine (SPSEL=1): saves to PSP, loads MSP, clears SPSEL=0
//
// Saves: r4-r11, lr (core callee-saved) + FPSCR, s16-s31 (FPU callee-saved)
// Total: 26 words = 104 bytes (8-byte aligned, AAPCS compliant)
//
// Frame layout (low -> high, as u32 array from SP):
//   [0..15]=s16..s31  [16]=FPSCR  [17..24]=r4..r11  [25]=lr
//
__attribute__((naked, noinline, used))
void context_switch(void** /*old_sp*/, void* /*new_sp*/) {
    asm volatile(
        // Save callee-saved regs to current stack (MSP or PSP)
        "push  {r4-r11, lr}       \n"
        "vmrs  r4, fpscr          \n"
        "push  {r4}               \n"
        "vpush {s16-s31}          \n"

        // Save current SP to *old_sp
        "str   sp, [r0]           \n"

        // Read CONTROL to determine active stack pointer
        "mrs   r4, control        \n"
        "tst   r4, #2             \n"  // test SPSEL bit
        "bne   1f                 \n"  // branch if PSP active

        // --- SPSEL=0 (MSP active, runner -> coroutine) ---
        "msr   psp, r1            \n"  // PSP = new_sp (coroutine's stack)
        "orr   r4, r4, #2         \n"  // set SPSEL bit
        "cpsid i                  \n"  // disable interrupts (race window)
        "msr   control, r4        \n"  // switch sp to PSP
        "isb                      \n"  // flush pipeline
        "cpsie i                  \n"  // re-enable interrupts
        "b     2f                 \n"

    "1:                           \n"
        // --- SPSEL=1 (PSP active, coroutine -> runner) ---
        "msr   msp, r1            \n"  // MSP = new_sp (runner's stack)
        "bic   r4, r4, #2         \n"  // clear SPSEL bit
        "cpsid i                  \n"  // disable interrupts (race window)
        "msr   control, r4        \n"  // switch sp to MSP
        "isb                      \n"  // flush pipeline
        "cpsie i                  \n"  // re-enable interrupts

    "2:                           \n"
        // Restore from new stack (SPSEL now selects it)
        "vpop  {s16-s31}          \n"
        "pop   {r4}               \n"
        "vmsr  fpscr, r4          \n"
        "pop   {r4-r11, pc}       \n"
    );
}

//=============================================================================
// Coroutine Platform — ARM Cortex-M7 context switching
//=============================================================================

void* CoroutinePlatformTeensy::createContext(void (*entry_fn)(),
                                              size_t stack_size) {
    if (stack_size < kMinCoroutineStackSize) {
        stack_size = kMinCoroutineStackSize;
    }

    fl::u8* stack = static_cast<fl::u8*>(malloc(stack_size));  // ok bare allocation
    if (!stack) {
        FL_WARN("CoroutinePlatformTeensy: Failed to allocate stack");
        return nullptr;
    }

    auto* state = new TeensyContextState();  // ok bare allocation
    state->stack_base = stack;
    state->stack_size = stack_size;

    // Stack canary at bottom (lowest address) — overflow corrupts this first
    fl::u32* canary = reinterpret_cast<fl::u32*>(stack);  // ok reinterpret cast
    *canary = kStackCanaryValue;

    // Initialize stack frame so context_switch will "return" into entry_fn
    void* stack_top = stack + stack_size;
    state->saved_sp = init_coroutine_stack(stack_top, entry_fn);

    return state;
}

void* CoroutinePlatformTeensy::createRunnerContext() {
    auto* state = new TeensyContextState();  // ok bare allocation
    // Runner uses main thread's MSP — saved_sp filled on first contextSwitch
    state->saved_sp = nullptr;
    state->stack_base = nullptr;
    state->stack_size = 0;
    return state;
}

void CoroutinePlatformTeensy::destroyContext(void* ctx) {
    if (!ctx) return;
    auto* state = static_cast<TeensyContextState*>(ctx);
    if (state->stack_base) {
        free(state->stack_base);  // ok bare allocation
    }
    delete state;  // ok bare allocation
}

void CoroutinePlatformTeensy::contextSwitch(void* from_ctx, void* to_ctx) {
    auto* from = static_cast<TeensyContextState*>(from_ctx);
    auto* to = static_cast<TeensyContextState*>(to_ctx);
    // Naked asm: saves regs to from->saved_sp, restores from to->saved_sp,
    // toggles MSP/PSP via CONTROL.SPSEL
    context_switch(&from->saved_sp, to->saved_sp);
}

bool CoroutinePlatformTeensy::inInterruptContext() const {
    fl::u32 ipsr;
    __asm__ volatile("MRS %0, ipsr" : "=r"(ipsr));
    return ipsr != 0;
}

bool CoroutinePlatformTeensy::checkStackHealth(void* ctx) const {
    if (!ctx) return true;
    auto* state = static_cast<TeensyContextState*>(ctx);
    if (!state->stack_base) return true;  // runner has no managed stack
    const fl::u32* canary =
        reinterpret_cast<const fl::u32*>(state->stack_base);  // ok reinterpret cast
    return *canary == kStackCanaryValue;
}

fl::u32 CoroutinePlatformTeensy::micros() const {
    return static_cast<fl::u32>(::micros());
}

// Register Teensy platform into generic coroutine_context code
namespace {
struct TeensyPlatformRegistrar {
    TeensyPlatformRegistrar() {
        ICoroutinePlatform::setInstance(
            &fl::Singleton<CoroutinePlatformTeensy>::instance());
    }
};
static TeensyPlatformRegistrar sTeensyPlatformRegistrar;
} // namespace

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeTeensy>::instance();
}

//=============================================================================
// Task Coroutine
//=============================================================================

TaskCoroutinePtr TaskCoroutineTeensy::create(
        fl::string name,
        TaskFunction function,
        size_t stack_size,
        u8 /*priority*/) {
    auto* ctx = CoroutineContext::create(fl::move(function), stack_size);
    if (!ctx) {
        FL_WARN("TaskCoroutineTeensy: Failed to create context for '" << name << "'");
        return nullptr;
    }

    TaskCoroutinePtr task(new TaskCoroutineTeensy());  // ok bare allocation
    auto* impl = static_cast<TaskCoroutineTeensy*>(task.get());
    impl->mName = fl::move(name);
    impl->mContext.reset(ctx);

    // Register with the global runner
    CoroutineRunner::instance().enqueue(ctx);

    return task;
}

TaskCoroutineTeensy::~TaskCoroutineTeensy() {
    stop();
    // mContext freed automatically by unique_ptr destructor
}

void TaskCoroutineTeensy::stop() {
    if (!mContext) return;

    mContext->stop_and_complete();

    // Remove from runner queue
    CoroutineRunner::instance().remove(mContext.get());
}

bool TaskCoroutineTeensy::isRunning() const {
    if (!mContext) return false;
    return !mContext->is_completed();
}

//=============================================================================
// Factory function — wired into platforms/coroutine.cpp.hpp dispatch
//=============================================================================

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int /*core_id*/) {
    return TaskCoroutineTeensy::create(fl::move(name), fl::move(function), stack_size, priority);
}

//=============================================================================
// Static exitCurrent — suspend back to runner and mark completed
//=============================================================================

void ICoroutineTask::exitCurrent() {
    CoroutineContext* ctx = CoroutineContext::runningCoroutine();
    if (ctx) {
        ctx->set_should_stop(true);
    }
    CoroutineContext::suspend();
    // Should never resume — runner marks completed and removes on should_stop.
    while (true) {}
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_TEENSY_4X
