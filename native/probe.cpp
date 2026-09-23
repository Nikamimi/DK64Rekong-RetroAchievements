#include "client_bridge.h"
#include "diagnostic_log.h"

#include <cstdint>
#include <cstdio>
#include <atomic>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__linux__)
#include <SDL.h>
#endif

#if defined(_WIN32)
#define DK64_RA_EXPORT extern "C" __declspec(dllexport)
#else
#define DK64_RA_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// librecomp validates this value before resolving any native exports.
DK64_RA_EXPORT const std::uint32_t recomp_api_version = 1;

namespace {
dk64_ra::ClientBridge bridge;
#if defined(_WIN32) || defined(__linux__)
// Observe the game's SDL2 key events without removing or changing them.
// Polling a key in a 30 Hz game loop can miss a brief keypress, so latch it.
struct SdlKeyEventPrefix {
    std::uint32_t type, timestamp, window_id;
    std::uint8_t state, repeat, padding2, padding3;
    std::int32_t scancode;
};
#ifdef _WIN32
using EventWatch = int(__cdecl*)(void*, void*);
using AddEventWatch = void(__cdecl*)(EventWatch, void*);
using DelEventWatch = void(__cdecl*)(EventWatch, void*);
#define DK64_RA_SDL_CALL __cdecl
#else
#define DK64_RA_SDL_CALL
#endif
std::atomic<bool> hotkey_pending{false};
std::atomic<bool> back_pending{false};
bool watch_registered = false;
int DK64_RA_SDL_CALL on_sdl_event(void*,
#ifdef _WIN32
                                  void* event) {
#else
                                  SDL_Event* event) {
#endif
    const auto* key = reinterpret_cast<const SdlKeyEventPrefix*>(event);
    if (key && key->type == 0x300U && !key->repeat) { // SDL_KEYDOWN
        if (key->scancode == 65) hotkey_pending.store(true, std::memory_order_relaxed); // F8
        if (key->scancode == 41) back_pending.store(true, std::memory_order_relaxed); // Escape
    }
    // SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLER_BUTTON_B. Observation only;
    // the event still reaches the game's controller/UI input pipeline.
    if (key && key->type == 0x651U &&
        reinterpret_cast<const std::uint8_t*>(event)[12] == 1) {
        back_pending.store(true, std::memory_order_relaxed);
    }
    return 1;
}
void register_hotkey_watch() {
    if (watch_registered) return;
#ifdef _WIN32
    HMODULE sdl = GetModuleHandleW(L"SDL2.dll");
    auto add = sdl ? reinterpret_cast<AddEventWatch>(GetProcAddress(sdl, "SDL_AddEventWatch")) : nullptr;
    if (add) {
        add(on_sdl_event, nullptr);
        watch_registered = true;
    }
#else
    SDL_AddEventWatch(on_sdl_event, nullptr);
    watch_registered = true;
#endif
}
struct WatchCleanup {
    ~WatchCleanup() {
#ifdef _WIN32
        HMODULE sdl = GetModuleHandleW(L"SDL2.dll");
        auto del = sdl ? reinterpret_cast<DelEventWatch>(GetProcAddress(sdl, "SDL_DelEventWatch")) : nullptr;
        if (del && watch_registered) del(on_sdl_event, nullptr);
#else
        if (watch_registered && SDL_WasInit(SDL_INIT_VIDEO))
            SDL_DelEventWatch(on_sdl_event, nullptr);
#endif
    }
} watch_cleanup;
#endif
}
// Called on the recompiled game thread. The ABI always supplies RDRAM and the
// MIPS register context, even though the corresponding mod imports have no args.
DK64_RA_EXPORT void dk64_ra_probe_init(std::uint8_t* rdram, void* context) {
#if defined(_WIN32) || defined(__linux__)
    register_hotkey_watch();
#endif
    // Recomp 1.0.2 passes MIPS a0 as r4, the fifth 64-bit GPR in recomp_context.
    // Do not read it when context is absent in a standalone native test.
    const std::uint32_t mode = context ?
        static_cast<std::uint32_t>(static_cast<const std::uint64_t*>(context)[4]) : 0;
    const std::uint32_t remember_signin = context ?
        static_cast<std::uint32_t>(static_cast<const std::uint64_t*>(context)[5]) : 0;
    const std::uint32_t notification_sound = context ?
        static_cast<std::uint32_t>(static_cast<const std::uint64_t*>(context)[6]) : 0;
#ifdef DK64_RA_MEMORY_DIAGNOSTICS
    // Diagnostic binaries are strictly offline even if local tracking is on.
    // This prevents another credential prompt and network
    // request during the address-parity test. Still mirror results to the log.
    dk64_ra::diagnostic_begin(true);
    if (bridge.initialize(rdram, dk64_ra::ClientBridge::Mode::DiagnosticsOffline)) {
        dk64_ra::diagnostic_log("[DK64 RA probe] Local memory diagnostic; offline, no sign-in or submissions.");
    }
#else
    const auto requested_mode = mode == 2 ? dk64_ra::ClientBridge::Mode::OnlineSoftcore :
        dk64_ra::ClientBridge::Mode::LocalTracking;
    dk64_ra::diagnostic_begin(true);
    if (bridge.initialize(rdram, requested_mode, remember_signin == 1,
                          notification_sound == 0)) {
        dk64_ra::diagnostic_log(requested_mode == dk64_ra::ClientBridge::Mode::OnlineSoftcore ?
            "[DK64 RA probe] Online softcore beta requested; Hardcore off, spectator off." :
            "[DK64 RA probe] Local tracking requested; Hardcore off, spectator on.");
    }
#endif
}

DK64_RA_EXPORT void dk64_ra_probe_frame(std::uint8_t* rdram, void* context) {
    (void)context;
    if (!rdram) {
        return;
    }
    bridge.on_frame(rdram);
    if (bridge.frames_seen() == 1) {
        dk64_ra::diagnostic_log("[DK64 RA probe] first simulated frame observed.");
    }
    // Silence after the first frame; never log gameplay RAM or account data.
}

namespace {
std::uint32_t argument(void* context, unsigned index) {
    return context ? static_cast<std::uint32_t>(static_cast<std::uint64_t*>(context)[index]) : 0;
}
void result(void* context, std::uint32_t value) {
    if (context) static_cast<std::uint64_t*>(context)[2] = value;
}
}

DK64_RA_EXPORT void dk64_ra_ui_revision(std::uint8_t*, void* context) {
    result(context, bridge.ui_revision());
}
DK64_RA_EXPORT void dk64_ra_ui_count(std::uint8_t*, void* context) {
    result(context, bridge.ui_count(static_cast<dk64_ra::ClientBridge::Filter>(argument(context, 4))));
}
DK64_RA_EXPORT void dk64_ra_ui_entry(std::uint8_t* rdram, void* context) {
    const auto encoded = argument(context, 4);
    result(context, bridge.ui_copy_entry(static_cast<dk64_ra::ClientBridge::Filter>(encoded >> 24),
                                         encoded & 0xFFFFFFU, argument(context, 5), rdram,
                                         argument(context, 6), argument(context, 7)) ? 1 : 0);
}
DK64_RA_EXPORT void dk64_ra_ui_badge(std::uint8_t* rdram, void* context) {
    const auto encoded = argument(context, 4);
    result(context, bridge.ui_copy_badge(static_cast<dk64_ra::ClientBridge::Filter>(encoded >> 24),
                                         encoded & 0xFFFFFFU, rdram, argument(context, 5),
                                         argument(context, 6)));
}
DK64_RA_EXPORT void dk64_ra_ui_summary(std::uint8_t* rdram, void* context) {
    result(context, bridge.ui_copy_summary(rdram, argument(context, 4), argument(context, 5)) ? 1 : 0);
}
DK64_RA_EXPORT void dk64_ra_ui_take_toast(std::uint8_t* rdram, void* context) {
    result(context, bridge.ui_take_toast(rdram, argument(context, 4), argument(context, 5)) ? 1 : 0);
}
DK64_RA_EXPORT void dk64_ra_ui_toast_description(std::uint8_t* rdram, void* context) {
    result(context, bridge.ui_copy_toast_description(rdram, argument(context, 4), argument(context, 5)) ? 1 : 0);
}
DK64_RA_EXPORT void dk64_ra_ui_toast_badge(std::uint8_t* rdram, void* context) {
    result(context, bridge.ui_copy_toast_badge(rdram, argument(context, 4), argument(context, 5)));
}

DK64_RA_EXPORT void dk64_ra_ui_hotkey(std::uint8_t*, void* context) {
#if defined(_WIN32)
    static bool was_down = false;
    static bool was_escape_down = false;
    DWORD foreground_pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
    const bool game_foreground = foreground_pid == GetCurrentProcessId();
    const SHORT state = game_foreground ? GetAsyncKeyState(VK_F8) : 0;
    const SHORT escape_state = game_foreground ? GetAsyncKeyState(VK_ESCAPE) : 0;
    const bool down = (state & 0x8000) != 0;
    const bool escape_down = (escape_state & 0x8000) != 0;
    const bool pressed = game_foreground &&
        (hotkey_pending.exchange(false, std::memory_order_relaxed) ||
         (down && !was_down) || (state & 1));
    was_down = down;
    const bool back = game_foreground &&
        (back_pending.exchange(false, std::memory_order_relaxed) ||
         (escape_down && !was_escape_down) || (escape_state & 1));
    was_escape_down = escape_down;
    result(context, (pressed ? 1 : 0) | (back ? 2 : 0));
#elif defined(__linux__)
    result(context, (hotkey_pending.exchange(false, std::memory_order_relaxed) ? 1 : 0) |
                    (back_pending.exchange(false, std::memory_order_relaxed) ? 2 : 0));
#else
    result(context, 0);
#endif
}

DK64_RA_EXPORT void dk64_ra_ui_reset_local(std::uint8_t*, void* context) {
    result(context, bridge.reset_local_progress() ? 1 : 0);
}
