#include "memory.h"

#include <algorithm>

std::uint32_t dk64_ra::MemoryView::read(std::uint32_t address,
                                         std::uint8_t* destination,
                                         std::uint32_t requested) const {
    if (!rdram_ || !destination || address >= kPhysicalRdramSize) {
        return 0;
    }

    const auto available = static_cast<std::uint32_t>(kPhysicalRdramSize - address);
    const auto count = std::min(requested, available);
    // The existing DK64 N64 set addresses the emulator's word-swapped,
    // little-endian RDRAM view. rcheevos interprets multi-byte operands from
    // that byte stream; normalizing it to N64 big-endian changes addresses.
    for (std::uint32_t i = 0; i < count; ++i) {
        destination[i] = rdram_[address + i];
    }
    return count;
}
