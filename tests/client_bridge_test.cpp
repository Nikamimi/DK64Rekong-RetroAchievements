#include "client_bridge.h"

#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    dk64_ra::ClientBridge bridge;
    if (bridge.initialize(nullptr) || bridge.ready()) {
        std::fputs("Null RDRAM unexpectedly initialized the client\n", stderr);
        return 1;
    }

    std::vector<std::uint8_t> rdram(dk64_ra::kPhysicalRdramSize, 0);
    if (!bridge.initialize(rdram.data(), dk64_ra::ClientBridge::Mode::DiagnosticsOffline) || !bridge.safe_mode()) {
        std::fputs("Offline rcheevos bridge failed to initialize safely\n", stderr);
        return 1;
    }
    bridge.on_frame(nullptr);
    bridge.on_frame(rdram.data());
    bridge.on_frame(rdram.data());
    if (bridge.frames_seen() != 2) {
        std::fputs("Game-frame callback count incorrect\n", stderr);
        return 1;
    }
    if (!bridge.initialize(rdram.data(), dk64_ra::ClientBridge::Mode::DiagnosticsOffline) || !bridge.safe_mode() || bridge.frames_seen() != 0) {
        std::fputs("New game did not reset offline runtime\n", stderr);
        return 1;
    }

    if (!bridge.initialize(rdram.data(), dk64_ra::ClientBridge::Mode::LocalTracking) ||
        !bridge.safe_mode() || bridge.mode() != dk64_ra::ClientBridge::Mode::LocalTracking) return 1;
    if (!bridge.initialize(rdram.data(), dk64_ra::ClientBridge::Mode::OnlineSoftcore) ||
        bridge.safe_mode() || bridge.mode() != dk64_ra::ClientBridge::Mode::OnlineSoftcore) return 1;

    std::puts("Offline rcheevos bridge: safe mode, frame calls, and reset passed");
    return 0;
}
