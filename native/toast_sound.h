#pragma once

#include <cstdint>
#include <vector>

namespace dk64_ra {

// Original, synthesized wood-percussion/marimba cadence. No game audio is
// sampled or bundled. The returned bytes are a PCM RIFF/WAVE image.
std::vector<std::uint8_t> make_toast_sound();
bool play_toast_sound();

} // namespace dk64_ra
