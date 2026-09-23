#include "guest_text.h"

#include <algorithm>

namespace dk64_ra {

bool write_guest_text(std::uint8_t* rdram, std::uint32_t guest_address,
                      std::uint32_t capacity, std::string_view text,
                      std::size_t mapped_bytes) {
    // Keep a bad import argument from writing into game RAM, a ROM mapping,
    // or outside the mod's 64 MiB extended RDRAM reservation.
    if (!rdram || !capacity || capacity > 256 ||
        guest_address < 0x81000000U || guest_address >= 0x85000000U) {
        return false;
    }
    const std::uint32_t offset = guest_address - 0x80000000U;
    if (capacity > 0x05000000U - offset ||
        static_cast<std::size_t>(offset) + capacity > mapped_bytes) {
        return false;
    }
    const auto count = std::min<std::size_t>(text.size(), capacity - 1);
    for (std::size_t i = 0; i < count; ++i) {
        rdram[(offset + i) ^ 3U] = static_cast<std::uint8_t>(text[i]);
    }
    rdram[(offset + count) ^ 3U] = 0;
    return true;
}

bool write_guest_bytes(std::uint8_t* rdram, std::uint32_t guest_address,
                       std::uint32_t capacity, const std::vector<std::uint8_t>& bytes,
                       std::size_t mapped_bytes) {
    if (!rdram || !capacity || capacity > 65536 || bytes.empty() || bytes.size() > capacity ||
        guest_address < 0x81000000U || guest_address >= 0x85000000U) {
        return false;
    }
    const std::uint32_t offset = guest_address - 0x80000000U;
    if (capacity > 0x05000000U - offset ||
        static_cast<std::size_t>(offset) + capacity > mapped_bytes) {
        return false;
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        rdram[(offset + i) ^ 3U] = bytes[i];
    }
    return true;
}

} // namespace dk64_ra
