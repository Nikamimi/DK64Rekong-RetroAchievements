#pragma once

#include "../include/ra_browser.h"
#include <cstdint>
#include <mutex>

namespace dk64_ra {
// SDL's event watch runs on another thread. Keep all browser state behind one
// lock, and discard input while the browser is closed or its reset modal owns it.
class BrowserInput {
public:
    void set_active(bool active) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_ != active) { active_ = active; pending_ = held_[0] = held_[1] = held_[2] = 0; }
    }
    void button(std::uint32_t bit, bool down, unsigned source = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || source >= 3) return;
        if (down) {
            if (!(held_[source] & bit)) pending_ |= bit;
            held_[source] |= bit;
        } else held_[source] &= ~bit;
    }
    void pulse(std::uint32_t bits) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_) pending_ |= bits;
    }
    void axis(unsigned axis, int value) {
        if (axis > 1) return;
        const auto negative = axis ? RA_INPUT_UP : RA_INPUT_LEFT;
        const auto positive = axis ? RA_INPUT_DOWN : RA_INPUT_RIGHT;
        button(negative, value < -16000, 2);
        button(positive, value > 16000, 2);
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = held_[0] = held_[1] = held_[2] = 0;
    }
    std::uint32_t poll(std::uint64_t now_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto result = pending_;
        pending_ = 0;
        const auto directions = (held_[0] | held_[1] | held_[2]) & 15U;
        if (result & 15U) repeat_at_ = now_ms + 320;
        else if (directions && now_ms >= repeat_at_) {
            result |= directions;
            repeat_at_ = now_ms + 100;
        }
        return result;
    }
private:
    std::mutex mutex_;
    bool active_ = false;
    // Independent keyboard, controller button and analog sources: a resting
    // stick must not cancel a held D-pad direction (or a keyboard key).
    std::uint32_t pending_ = 0, held_[3]{};
    std::uint64_t repeat_at_ = 0;
};
}
