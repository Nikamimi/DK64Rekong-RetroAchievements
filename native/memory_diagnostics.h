#pragma once

#include <cstdint>

namespace dk64_ra {

// Local-only build option: count comparisons against selected game-state
// addresses. Never record the contents of RDRAM or account information.
#ifdef DK64_RA_MEMORY_DIAGNOSTICS
void memory_diagnostics_frame(const std::uint8_t* rdram, std::uint64_t frames_seen);
#else
inline void memory_diagnostics_frame(const std::uint8_t*, std::uint64_t) {}
#endif

} // namespace dk64_ra
