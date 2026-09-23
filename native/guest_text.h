#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace dk64_ra {

// Mod code/data lives in extended RDRAM starting at guest 0x81000000.
// Like MEM_B in librecomp, guest bytes are XOR 3 in the host allocation.
// Never accept an arbitrary host pointer from a MIPS import.
bool write_guest_text(std::uint8_t* rdram, std::uint32_t guest_address,
                      std::uint32_t capacity, std::string_view text,
                      std::size_t mapped_bytes);

// Binary texture bytes use the same guest byte order and restricted extended
// RDRAM range. No terminator is added and an oversized image is rejected.
bool write_guest_bytes(std::uint8_t* rdram, std::uint32_t guest_address,
                       std::uint32_t capacity, const std::vector<std::uint8_t>& bytes,
                       std::size_t mapped_bytes);

} // namespace dk64_ra
