#include "toast_sound.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#ifdef __linux__
#include <SDL.h>
#include <cstdlib>
#endif

int main(int argc, char** argv) {
    const auto wave = dk64_ra::make_toast_sound();
    if (wave.size() < 44 || std::memcmp(wave.data(), "RIFF", 4) ||
        std::memcmp(wave.data() + 8, "WAVEfmt ", 8) ||
        std::memcmp(wave.data() + 36, "data", 4)) return 1;
    const auto u32 = [&](unsigned offset) {
        return std::uint32_t(wave[offset]) | (std::uint32_t(wave[offset + 1]) << 8) |
               (std::uint32_t(wave[offset + 2]) << 16) | (std::uint32_t(wave[offset + 3]) << 24);
    };
    if (u32(4) + 8 != wave.size() || u32(24) != 22050 ||
        u32(40) + 44 != wave.size() || wave[34] != 16 || wave[22] != 1) return 2;
    bool audible = false;
    for (std::size_t i = 44; i + 1 < wave.size(); i += 2) {
        const auto sample = static_cast<std::int16_t>(wave[i] | (wave[i + 1] << 8));
        if (sample > 2000 || sample < -2000) audible = true;
    }
    if (!audible || wave != dk64_ra::make_toast_sound()) return 3;
#ifdef __linux__
    // Exercise the playback route without requiring speakers in CI/WSL.
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_AUDIO) != 0 || !dk64_ra::play_toast_sound()) return 5;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
    if (argc == 2) {
        std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(wave.data()), wave.size());
        if (!output) return 4;
    }
    std::puts("Original toast WAV header, signal and determinism passed");
    return 0;
}
