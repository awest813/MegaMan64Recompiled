#include <cstdint>

#include "recomp.h"
#include "zelda_config.h"

namespace {
    // The runtime hooks needed for a safe full-state quicksave are not available
    // in this project yet, so keep the public entry points as explicit no-ops.
    constexpr bool quicksave_supported = false;
}

void zelda64::quicksave_save() {
    (void)quicksave_supported;
}

void zelda64::quicksave_load() {
    (void)quicksave_supported;
}

extern "C" void recomp_handle_quicksave_actions(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

extern "C" void recomp_handle_quicksave_actions_main(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}
