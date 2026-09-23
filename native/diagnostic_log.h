#pragma once

namespace dk64_ra {

// Development local tracking only. Mirrors our own diagnostic lines to a file next
// to the native mod, regardless of how the game process was launched.
void diagnostic_begin(bool local_tracking);
void diagnostic_log(const char* format, ...);

} // namespace dk64_ra
