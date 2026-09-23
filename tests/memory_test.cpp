#include "memory.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <vector>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "line %d: %s failed\n", __LINE__, #expression); \
            return 1; \
        } \
    } while (0)

int main() {
    std::vector<std::uint8_t> rdram(dk64_ra::kPhysicalRdramSize, 0);
    const dk64_ra::MemoryView memory(rdram.data());
    // DK64's RDRAM is word-swapped: the existing RA set addresses this raw
    // emulator byte layout, not N64 big-endian byte positions.
    rdram[0] = 0x78;
    rdram[1] = 0x56;
    rdram[2] = 0x34;
    rdram[3] = 0x12;
    std::array<std::uint8_t, 4> bytes{};
    CHECK(memory.read(0, bytes.data(), 4) == 4);
    CHECK((bytes == std::array<std::uint8_t, 4>{0x78, 0x56, 0x34, 0x12}));
    CHECK(memory.read(1, bytes.data(), 2) == 2);
    CHECK(bytes[0] == 0x56 && bytes[1] == 0x34);

    // Regression: the official DK Rap trigger reads a byte at 0x755317 and
    // a little-endian 16-bit timer at 0x75531E. Do not XOR these addresses.
    rdram[0x755317] = 2;
    rdram[0x75531E] = 0x44;
    rdram[0x75531F] = 0x16;
    CHECK(memory.read(0x755317, bytes.data(), 1) == 1 && bytes[0] == 2);
    CHECK(memory.read(0x75531E, bytes.data(), 2) == 2);
    CHECK(bytes[0] == 0x44 && bytes[1] == 0x16);

    rdram[dk64_ra::kPhysicalRdramSize - 2] = 0xBE;
    rdram[dk64_ra::kPhysicalRdramSize - 1] = 0xEF;
    bytes.fill(0xAA);
    CHECK(memory.read(static_cast<std::uint32_t>(dk64_ra::kPhysicalRdramSize - 2),
                      bytes.data(), 4) == 2);
    CHECK(bytes[0] == 0xBE && bytes[1] == 0xEF && bytes[2] == 0xAA);
    CHECK(memory.read(static_cast<std::uint32_t>(dk64_ra::kPhysicalRdramSize),
                      bytes.data(), 1) == 0);
    CHECK(memory.read(UINT32_MAX, bytes.data(), UINT32_MAX) == 0);
    CHECK(memory.read(0, nullptr, 1) == 0);
    CHECK(memory.read(0, bytes.data(), 0) == 0);
    CHECK(dk64_ra::MemoryView(nullptr).read(0, bytes.data(), 4) == 0);
}
