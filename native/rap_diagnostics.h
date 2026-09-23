#pragma once

extern "C" {
#include "rc_client.h"
}

namespace dk64_ra {

// Local diagnostic build only: inspect the official runtime's condition status
// without recording the user's RAM contents or submitting any unlocks.
#ifdef DK64_RA_RAP_DIAGNOSTICS
void rap_diagnostics_loaded(rc_client_t* client);
void rap_diagnostics_frame(rc_client_t* client, unsigned long long frames_seen);
#else
inline void rap_diagnostics_loaded(rc_client_t*) {}
inline void rap_diagnostics_frame(rc_client_t*, unsigned long long) {}
#endif

} // namespace dk64_ra
