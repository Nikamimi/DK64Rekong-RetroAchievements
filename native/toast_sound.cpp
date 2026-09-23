#include "toast_sound.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#elif defined(__linux__)
#include <SDL.h>
#endif

namespace dk64_ra {
namespace {
constexpr unsigned kRate = 22050;
constexpr double kDuration = 0.82;
constexpr double kPi = 3.14159265358979323846;

void put16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}
void put32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    put16(bytes, static_cast<std::uint16_t>(value));
    put16(bytes, static_cast<std::uint16_t>(value >> 16));
}
void word(std::vector<std::uint8_t>& bytes, const char* value) {
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value[i]));
}
double marimba(double t, double start, double frequency, double strength) {
    const double age = t - start;
    if (age < 0 || age > 0.45) return 0;
    const double attack = std::min(1.0, age * 450.0);
    const double fundamental = std::sin(2 * kPi * frequency * age);
    const double woody_partial = 0.35 * std::sin(2 * kPi * frequency * 3.0 * age) * std::exp(-20 * age);
    return strength * attack * std::exp(-9.5 * age) * (fundamental + woody_partial);
}
double bongo(double t, double start, double frequency, double strength) {
    const double age = t - start;
    if (age < 0 || age > 0.16) return 0;
    // A descending pitch and brief dry click make a hand-drum-like attack.
    const double phase = 2 * kPi * (frequency * age - frequency * 0.43 * age * age);
    const double click = std::sin(2 * kPi * 1733 * age) * std::exp(-115 * age);
    return strength * (std::sin(phase) * std::exp(-34 * age) + 0.20 * click);
}
}

std::vector<std::uint8_t> make_toast_sound() {
    const auto samples = static_cast<std::uint32_t>(kRate * kDuration);
    const auto data_bytes = samples * 2;
    std::vector<std::uint8_t> wave;
    wave.reserve(44 + data_bytes);
    word(wave, "RIFF"); put32(wave, 36 + data_bytes); word(wave, "WAVE");
    word(wave, "fmt "); put32(wave, 16); put16(wave, 1); put16(wave, 1);
    put32(wave, kRate); put32(wave, kRate * 2); put16(wave, 2); put16(wave, 16);
    word(wave, "data"); put32(wave, data_bytes);
    for (std::uint32_t i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / kRate;
        const double ending = std::min(1.0, (kDuration - t) * 11.0);
        const double signal =
            bongo(t, 0.00, 175, 0.47) + bongo(t, 0.155, 235, 0.31) +
            bongo(t, 0.325, 170, 0.40) + bongo(t, 0.51, 255, 0.29) +
            marimba(t, 0.045, 523.25, 0.32) + marimba(t, 0.205, 659.25, 0.31) +
            marimba(t, 0.365, 783.99, 0.32) + marimba(t, 0.535, 1046.5, 0.40);
        const auto pcm = static_cast<std::int16_t>(std::clamp(signal * ending, -0.95, 0.95) * 32767);
        put16(wave, static_cast<std::uint16_t>(pcm));
    }
    return wave;
}

bool play_toast_sound() {
#ifdef _WIN32
    struct SoundBuffer {
        std::vector<std::uint8_t> wave = make_toast_sound();
        ~SoundBuffer() { PlaySoundW(nullptr, nullptr, 0); }
    };
    static const SoundBuffer sound;
    // Keep the memory valid for asynchronous playback. Do not fall back to a
    // system alert if an audio device is unavailable.
    return PlaySoundA(reinterpret_cast<const char*>(sound.wave.data()), nullptr,
                      SND_MEMORY | SND_ASYNC | SND_NODEFAULT) != 0;
#elif defined(__linux__)
    // Queue the original PCM jingle on its own SDL device; never block a game
    // frame for the duration of playback or borrow the game's audio queue.
    if (!SDL_WasInit(SDL_INIT_AUDIO)) return false;
    struct SoundDevice {
        SDL_AudioDeviceID id = 0;
        std::vector<std::uint8_t> wave = make_toast_sound();
        ~SoundDevice() { if (id && SDL_WasInit(SDL_INIT_AUDIO)) SDL_CloseAudioDevice(id); }
    };
    static SoundDevice sound;
    if (!sound.id) {
        SDL_AudioSpec desired{};
        desired.freq = static_cast<int>(kRate);
        desired.format = AUDIO_S16LSB;
        desired.channels = 1;
        desired.samples = 1024;
        sound.id = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
        if (!sound.id) return false;
    }
    SDL_ClearQueuedAudio(sound.id);
    if (SDL_QueueAudio(sound.id, sound.wave.data() + 44,
                       static_cast<Uint32>(sound.wave.size() - 44)) != 0) return false;
    SDL_PauseAudioDevice(sound.id, 0);
    return true;
#else
    return false;
#endif
}

} // namespace dk64_ra
