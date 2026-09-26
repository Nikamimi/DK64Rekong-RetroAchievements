#include <dlfcn.h>
#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>
#include <cstdlib>
#include "../include/ra_browser.h"

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        std::fprintf(stderr, "Linux native companion did not load: %s\n", dlerror());
        return 2;
    }
    auto* version = static_cast<const std::uint32_t*>(dlsym(library, "recomp_api_version"));
    if (!version || *version != 1) return 3;
    for (const char* name : {"dk64_ra_probe_init", "dk64_ra_probe_frame",
                              "dk64_ra_ui_revision", "dk64_ra_ui_count", "dk64_ra_ui_entry",
                              "dk64_ra_ui_badge", "dk64_ra_ui_summary", "dk64_ra_ui_take_toast",
                              "dk64_ra_ui_toast_description", "dk64_ra_ui_toast_badge",
                              "dk64_ra_ui_hotkey", "dk64_ra_ui_reset_local", "dk64_ra_ui_input",
                              "dk64_ra_ui_account", "dk64_ra_ui_account_action"}) {
        if (!dlsym(library, name)) {
            std::fprintf(stderr, "Linux native export missing: %s\n", name);
            return 4;
        }
    }
    // SDL_PushEvent reaches the same non-consuming event watch that the game
    // calls into; this checks the Linux F8 and Escape latch without a game ROM.
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) return 5;
    using NativeCall = void (*)(std::uint8_t*, void*);
    const auto init = reinterpret_cast<NativeCall>(dlsym(library, "dk64_ra_probe_init"));
    const auto hotkey = reinterpret_cast<NativeCall>(dlsym(library, "dk64_ra_ui_hotkey"));
    std::vector<std::uint8_t> memory(8U * 1024U * 1024U);
    std::uint64_t context[32]{};
    context[4] = 1; // Local mode; no ROM/network access before the first frame.
    init(memory.data(), context);
    SDL_Event key{};
    key.type = SDL_KEYDOWN;
    key.key.keysym.scancode = SDL_SCANCODE_F8;
    if (SDL_PushEvent(&key) != 1) return 6;
    hotkey(nullptr, context);
    if (context[2] != 1) return 7;
    hotkey(nullptr, context);
    if (context[2] != 0) return 8;
    key.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
    if (SDL_PushEvent(&key) != 1) return 9;
    hotkey(nullptr, context);
    if (context[2] != 2) return 10;
    SDL_Event button{};
    button.type = SDL_CONTROLLERBUTTONDOWN;
    button.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    if (SDL_PushEvent(&button) != 1) return 11;
    hotkey(nullptr, context);
    if (context[2] != 2) return 12;
    const auto input = reinterpret_cast<NativeCall>(dlsym(library, "dk64_ra_ui_input"));
    context[4] = 3;
    input(nullptr, context);
    key.key.keysym.scancode = SDL_SCANCODE_RIGHT;
    if (SDL_PushEvent(&key) != 1) return 13;
    key.type = SDL_KEYUP;
    if (SDL_PushEvent(&key) != 1) return 14;
    context[4] = 1;
    input(nullptr, context);
    if (context[2] != RA_INPUT_RIGHT) return 15;
    button.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    if (SDL_PushEvent(&button) != 1) return 16;
    input(nullptr, context);
    if (context[2] != RA_INPUT_DOWN) return 17;
    context[4] = 0;
    input(nullptr, context);
    context[4] = 3;
    input(nullptr, context);
    SDL_Event motion{};
    motion.type = SDL_MOUSEMOTION;
    if (SDL_PushEvent(&motion) != 1) return 18;
    context[4] = 1;
    input(nullptr, context);
    if (context[2] != RA_INPUT_POINTER) return 19;
    SDL_Event axis{};
    axis.type = SDL_CONTROLLERAXISMOTION;
    axis.caxis.axis = SDL_CONTROLLER_AXIS_LEFTX;
    axis.caxis.value = -25000;
    if (SDL_PushEvent(&axis) != 1) return 20;
    input(nullptr, context);
    if (context[2] != RA_INPUT_LEFT) return 21;
    context[4] = 3;
    input(nullptr, context);
    for (const auto scan : {SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_SPACE, SDL_SCANCODE_UNKNOWN}) {
        key.type = SDL_KEYDOWN;
        key.key.keysym.scancode = scan;
        key.key.keysym.sym = SDLK_RETURN;
        if (SDL_PushEvent(&key) != 1) return 22;
        key.type = SDL_KEYUP;
        if (SDL_PushEvent(&key) != 1) return 23;
        context[4] = 1;
        input(nullptr, context);
        if (context[2] != RA_INPUT_ACCEPT) return 24;
    }
    button.type = SDL_CONTROLLERBUTTONDOWN;
    button.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    if (SDL_PushEvent(&button) != 1) return 25;
    button.type = SDL_CONTROLLERBUTTONUP;
    if (SDL_PushEvent(&button) != 1) return 26;
    input(nullptr, context);
    if (context[2] != RA_INPUT_ACCEPT) return 27;
    dlclose(library);
    SDL_Quit();
    std::puts("Linux native library loads with all exports and SDL keyboard, pointer, D-pad and stick input");
    return 0;
}
