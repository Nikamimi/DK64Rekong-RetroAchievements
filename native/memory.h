#pragma once

#include <cstddef>
#include <cstdint>

namespace dk64_ra {
constexpr std::size_t kPhysicalRdramSize = 8U * 1024U * 1024U;

// Rekongpiled stores N64 words in the host's little-endian layout. RA expects
// consecutive physical N64 bytes; a byte read from RAM address a is at a ^ 3.
class MemoryView {
public:
    explicit MemoryView(const std::uint8_t* rdram) : rdram_(rdram) {}

    // Returns the number of bytes read, possibly short at the RAM boundary.
    std::uint32_t read(std::uint32_t address, std::uint8_t* destination,
                       std::uint32_t requested) const;

private:
    const std::uint8_t* rdram_;
};
} // namespace dk64_ra
