#include "modding.h"
#include "ra_ui.h"

/* The special "." dependency resolves these functions in this mod's native library. */
RECOMP_IMPORT("*", unsigned long recomp_get_config_u32(const char* key));
RECOMP_IMPORT(".", void dk64_ra_probe_init(unsigned long mode, unsigned long notification_sound));
RECOMP_IMPORT(".", void dk64_ra_probe_frame(void));

/* The upstream per-frame event fires before DK64 updates its gameplay state.
 * Defer RA evaluation until the loop's later function returns, so one-frame
 * flags set during the frame can be seen by the achievement runtime. */
static unsigned int frame_pending;

RECOMP_CALLBACK("*", recomp_on_init)
void dk64_ra_on_init(void) {
    frame_pending = 0;
    /* A fresh setting ID prevents a saved, older config index from silently
     * enabling submissions. Online is the explicit default for this build. */
    dk64_ra_probe_init(recomp_get_config_u32("tracking_mode_v2") == 0 ? 2 : 1,
                       recomp_get_config_u32("notification_sound"));
    dk64_ra_ui_init();
}

RECOMP_CALLBACK("*", dk64recomp_every_frame)
void dk64_ra_on_frame(void) {
    frame_pending = 1;
}

/* This function is called once near the end of the normal game loop. A rare
 * extra call can occur while draining the deferred-work queue; the pending
 * flag prevents double evaluation in one frame. */
RECOMP_HOOK_RETURN("func_global_asm_80611730")
void dk64_ra_after_game_logic(void) {
    if (frame_pending) {
        frame_pending = 0;
        dk64_ra_probe_frame();
        dk64_ra_ui_frame();
    }
}
