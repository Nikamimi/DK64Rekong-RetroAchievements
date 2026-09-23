#include <dlfcn.h>
#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>
#include <cstdlib>

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
                              "dk64_ra_ui_hotkey", "dk64_ra_ui_reset_local"}) {
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
    dlclose(library);
    SDL_Quit();
    std::puts("Linux native library loads with all exports and SDL F8/Esc/B latches");
    return 0;
}
