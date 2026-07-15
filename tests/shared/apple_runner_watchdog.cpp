// ok standalone
// Apple runner watchdog implementation. Kept in a .cpp file so POSIX system
// headers do not leak into the public FastLED header surface.

#include "platforms/posix/is_posix.h"

#ifdef FL_IS_APPLE

// IWYU pragma: begin_keep
#include <csignal>
#include <cstddef> // ok include - private host-runner implementation
#include <cstdlib> // ok include - must call system getenv/atof without libfastled
#include <cstring> // ok include - signal setup must not add libfastled linkage
#include <unistd.h>
// IWYU pragma: end_keep

#include "fl/stl/int.h"
#include "platforms/apple/runner_watchdog.hpp"

namespace apple_runner_watchdog {

static volatile sig_atomic_t g_active = 0;
static volatile sig_atomic_t g_phase = static_cast<sig_atomic_t>(Phase::kStartup);

template <size_t N>
static void write_literal(const char (&message)[N]) FL_NO_EXCEPT {
    (void)::write(STDERR_FILENO, message, N - 1);
}

static void write_phase(Phase phase) FL_NO_EXCEPT {
    switch (phase) {
    case Phase::kStartup:
        write_literal("[FASTLED RUNNER] phase: startup\n");
        break;
    case Phase::kCrashHandlerSetup:
        write_literal("[FASTLED RUNNER] phase: crash-handler-setup\n");
        break;
    case Phase::kPathResolution:
        write_literal("[FASTLED RUNNER] phase: path-resolution\n");
        break;
    case Phase::kDynamicLoad:
        write_literal("[FASTLED RUNNER] phase: dynamic-load\n");
        break;
    case Phase::kSymbolLookup:
        write_literal("[FASTLED RUNNER] phase: symbol-lookup\n");
        break;
    case Phase::kInvocation:
        write_literal("[FASTLED RUNNER] phase: invocation\n");
        break;
    case Phase::kTeardown:
        write_literal("[FASTLED RUNNER] phase: teardown\n");
        break;
    case Phase::kCompleted:
        write_literal("[FASTLED RUNNER] phase: completed\n");
        break;
    }
}

void set_phase(Phase phase) FL_NO_EXCEPT {
    g_phase = static_cast<sig_atomic_t>(phase);
    write_phase(phase);
}

static void alarm_handler(int) FL_NO_EXCEPT {
    if (!g_active) {
        return;
    }

    write_literal("[FASTLED RUNNER] watchdog timeout; last observed phase follows\n");
    write_phase(static_cast<Phase>(g_phase));
    _exit(1);
}

void setup(double timeout_seconds) FL_NO_EXCEPT {
    const char* disable_env = ::getenv("FASTLED_DISABLE_TIMEOUT_WATCHDOG");
    if (disable_env &&
        (::strcmp(disable_env, "1") == 0 || ::strcmp(disable_env, "true") == 0)) { // ok ctype
        write_literal("[FASTLED RUNNER] watchdog disabled\n");
        return;
    }

    const char* timeout_env = ::getenv("FASTLED_TEST_TIMEOUT");
    if (timeout_env) {
        const double parsed = ::atof(timeout_env);
        if (parsed > 0.0) {
            timeout_seconds = parsed;
        }
    }

    struct sigaction action;
    ::memset(&action, 0, sizeof(action)); // ok ctype
    action.sa_handler = alarm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGALRM, &action, nullptr) != 0) {
        write_literal("[FASTLED RUNNER] failed to install watchdog\n");
        return;
    }

    g_active = 1;
    fl::u32 timeout = static_cast<fl::u32>(timeout_seconds);
    if (timeout == 0) {
        timeout = 1;
    }
    alarm(timeout);
    write_literal("[FASTLED RUNNER] watchdog armed\n");
}

void cancel() FL_NO_EXCEPT {
    if (!g_active) {
        return;
    }
    g_active = 0;
    alarm(0);
    // Keep the handler installed until process exit. alarm(0) does not clear a
    // signal that is already pending; restoring SIG_DFL here could turn that
    // harmless late delivery into process termination after a successful run.
}

} // namespace apple_runner_watchdog

#endif // FL_IS_APPLE
