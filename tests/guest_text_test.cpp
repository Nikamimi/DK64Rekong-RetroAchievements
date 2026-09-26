#include "guest_text.h"

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>

int main() {
    std::vector<std::uint8_t> memory(0x01000100, 0xA5);
    constexpr std::uint32_t address = 0x81000020U;
    if (!dk64_ra::write_guest_text(memory.data(), address, 5, "Donkey", memory.size())) {
        return 1;
    }
    const char expected[] = "Donk";
    for (unsigned i = 0; i < 5; ++i) {
        if (memory[((address & 0x1FFFFFFFU) + i) ^ 3U] !=
            static_cast<std::uint8_t>(expected[i])) {
            std::fputs("Guest byte order or truncation incorrect\n", stderr);
            return 1;
        }
    }
    if (dk64_ra::write_guest_text(memory.data(), 0x80000000U, 5, "bad", memory.size()) ||
        dk64_ra::write_guest_text(memory.data(), 0x810000FEU, 5, "bad", memory.size()) ||
        dk64_ra::write_guest_text(memory.data(), address, 0, "bad", memory.size()) ||
        dk64_ra::write_guest_text(memory.data(), address, 1025, "bad", memory.size())) {
        std::fputs("Invalid guest destination was accepted\n", stderr);
        return 1;
    }
    const std::vector<std::uint8_t> png_bytes{137, 80, 78, 71, 0, 255};
    if (!dk64_ra::write_guest_bytes(memory.data(), address, 16, png_bytes, memory.size())) {
        std::fputs("Binary guest copy failed\n", stderr);
        return 1;
    }
    for (unsigned i = 0; i < png_bytes.size(); ++i) {
        if (memory[((address & 0x1FFFFFFFU) + i) ^ 3U] != png_bytes[i]) return 1;
    }
    if (dk64_ra::write_guest_bytes(memory.data(), 0x80000000U, 16, png_bytes, memory.size()) ||
        dk64_ra::write_guest_bytes(memory.data(), address, 4, png_bytes, memory.size()) ||
        dk64_ra::write_guest_bytes(memory.data(), 0x810000FEU, 16, png_bytes, memory.size())) {
        std::fputs("Invalid binary destination was accepted\n", stderr);
        return 1;
    }
    memory.resize(0x01001000, 0xA5);
    const std::string description(700, 'x');
    if (!dk64_ra::write_guest_text(memory.data(), address, 1024, description, memory.size())) return 1;
    for (unsigned i = 0; i <= description.size(); ++i) {
        if (memory[((address & 0x1FFFFFFFU) + i) ^ 3U] != (i == description.size() ? 0 : 'x')) return 1;
    }
    if (dk64_ra::write_guest_text(memory.data(), 0x81000FFFU, 1024, description, memory.size())) return 1;
    std::puts("Extended RDRAM text copy, full descriptions and bounds passed");
    return 0;
}
