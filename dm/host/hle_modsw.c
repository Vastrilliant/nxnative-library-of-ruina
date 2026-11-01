// Mods that patch the game's code (mods/<id>/exefs, dm/tools/nso_switch.py): the rewriter turns each patched word into a stub that reads the mod's byte in the page after the key page and runs the patched word when it is 1.
// The bytes are written here when the game starts, from the mods' env switches (the launcher's Mods section sets them, Documents/env.txt wins over it), the same moment as the frame step state (loader.c dm_fps_state_apply):
// on iOS the loading runs at app launch, before the launcher has set them. A build with no switchable patches has no page, and with every switch off the game runs as unpatched.
// scripts/apply_mods.py writes hle_modsw_gen.h (the env name of each byte); without it there are no mods with patches
#include "dm.h"
#include "hle.h"
#if __has_include("hle_modsw_gen.h")
#include "hle_modsw_gen.h"
#else
#define MODSW_N 0
static const char *const k_modsw_env[1] = { "" };
#endif

void dm_modsw_apply(void) {
    if (!dm_modsw) return;
    for (int i = 0; i < MODSW_N; i++) {
        const char *v = getenv(k_modsw_env[i]);
        const int on = v && v[0] == '1' && !v[1];
        dm_modsw[0x100 + i] = (uint8_t)on;
        if (on) LOG("[mod] %s: on", k_modsw_env[i]);
    }
}
