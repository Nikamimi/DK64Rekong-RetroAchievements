#include "memory_diagnostics.h"

#include "diagnostic_log.h"
#include "memory.h"

#include <array>

namespace dk64_ra {
namespace {
constexpr std::uint32_t kStateStart = 0x755314;
constexpr std::size_t kStateBytes = 8;
constexpr std::uint32_t kTimerStart = 0x75531C;
std::array<std::uint64_t, kStateBytes> canonical_twos{};
std::array<std::uint64_t, kStateBytes> host_twos{};
std::array<std::uint64_t, 2> timer_at_least_42{};
std::array<std::uint64_t, 2> timer_changed{};
std::array<std::uint16_t, 2> previous_timer{};
bool have_previous = false;
} // namespace

void memory_diagnostics_frame(const std::uint8_t* rdram, std::uint64_t frames_seen) {
    std::array<std::uint8_t, kStateBytes + 4> canonical{};
    if (MemoryView(rdram).read(kStateStart, canonical.data(),
                               static_cast<std::uint32_t>(canonical.size())) != canonical.size()) {
        return;
    }
    for (std::size_t i = 0; i < kStateBytes; ++i) {
        canonical_twos[i] += canonical[i] == 2;
        host_twos[i] += rdram[kStateStart + i] == 2;
    }
    for (std::size_t i = 0; i < 2; ++i) {
        const auto offset = kStateBytes + 2 * i;
        const auto timer = static_cast<std::uint16_t>((canonical[offset] << 8) | canonical[offset + 1]);
        timer_at_least_42[i] += timer >= 42;
        timer_changed[i] += have_previous && timer != previous_timer[i];
        previous_timer[i] = timer;
    }
    have_previous = true;
    if (frames_seen % 900 != 0) {
        return;
    }
    diagnostic_log("[DK64 RA memdiag] frame %llu; canonical 0x755314..31B equal 2: %llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                   static_cast<unsigned long long>(frames_seen),
                   static_cast<unsigned long long>(canonical_twos[0]),
                   static_cast<unsigned long long>(canonical_twos[1]),
                   static_cast<unsigned long long>(canonical_twos[2]),
                   static_cast<unsigned long long>(canonical_twos[3]),
                   static_cast<unsigned long long>(canonical_twos[4]),
                   static_cast<unsigned long long>(canonical_twos[5]),
                   static_cast<unsigned long long>(canonical_twos[6]),
                   static_cast<unsigned long long>(canonical_twos[7]));
    diagnostic_log("[DK64 RA memdiag] host byte order equal 2: %llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu; timer >=42 at 31C/31E: %llu/%llu; changes: %llu/%llu",
                   static_cast<unsigned long long>(host_twos[0]),
                   static_cast<unsigned long long>(host_twos[1]),
                   static_cast<unsigned long long>(host_twos[2]),
                   static_cast<unsigned long long>(host_twos[3]),
                   static_cast<unsigned long long>(host_twos[4]),
                   static_cast<unsigned long long>(host_twos[5]),
                   static_cast<unsigned long long>(host_twos[6]),
                   static_cast<unsigned long long>(host_twos[7]),
                   static_cast<unsigned long long>(timer_at_least_42[0]),
                   static_cast<unsigned long long>(timer_at_least_42[1]),
                   static_cast<unsigned long long>(timer_changed[0]),
                   static_cast<unsigned long long>(timer_changed[1]));
}

} // namespace dk64_ra
