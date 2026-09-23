#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace dk64_ra {

// Lazily fetches public RA badge PNGs, without using the authenticated API
// transport or blocking the recompiled game thread. Failed badges remain a
// placeholder for this session; there is no unbounded retry loop.
class BadgeCache {
public:
    BadgeCache() = default;
    ~BadgeCache();
    BadgeCache(const BadgeCache&) = delete;
    BadgeCache& operator=(const BadgeCache&) = delete;

    static bool valid_url(std::string_view url);
    static bool valid_png(const std::vector<std::uint8_t>& data);
    std::uint32_t revision() const { return revision_.load(std::memory_order_relaxed); }
    std::uint32_t copy_or_queue(std::string_view url, std::uint8_t* rdram,
                                std::uint32_t guest_address, std::uint32_t capacity);

private:
    enum class State { Pending, Ready, Failed };
    struct Image {
        State state = State::Pending;
        std::vector<std::uint8_t> bytes;
    };
    void run();
    static std::vector<std::uint8_t> download(std::string_view url);

    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::unordered_map<std::string, Image> images_;
    std::deque<std::string> queued_;
    std::thread worker_;
    bool stopping_ = false;
    std::atomic<std::uint32_t> revision_{0};
};

} // namespace dk64_ra
