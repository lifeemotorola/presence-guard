/* TypeBloom: a rolling count of typing-like key presses, never their contents.
 * Copyright (C) 2026 Emmanuel Suah. SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace typebloom {

// Virtual-key values are used only to decide whether to increment the meter;
// the key value is not passed to or stored by Meter. These constants are stable
// Windows VK_* values, written numerically so this policy can be tested on any OS.
struct Modifiers {
  bool control = false;
  bool alt = false;
  bool windows = false;
  bool right_alt = false;
};

// Track only modifier state, so key-up events can reliably exclude shortcuts.
// This stores no ordinary key values. The hook runs before GetAsyncKeyState is
// updated for its current event, so observing modifier transitions is important.
struct ModifierState {
  bool left_control = false, right_control = false, generic_control = false;
  bool left_alt = false, right_alt = false, generic_alt = false;
  bool left_windows = false, right_windows = false;

  void observe(unsigned int vk, bool down) {
    switch (vk) {
      case 0x11: generic_control = down; break; // VK_CONTROL
      case 0xA2: left_control = down; break;
      case 0xA3: right_control = down; break;
      case 0x12: generic_alt = down; break; // VK_MENU
      case 0xA4: left_alt = down; break;
      case 0xA5: right_alt = down; break;
      case 0x5B: left_windows = down; break;
      case 0x5C: right_windows = down; break;
      default: break;
    }
  }
  Modifiers current() const {
    return {left_control || right_control || generic_control,
            left_alt || right_alt || generic_alt,
            left_windows || right_windows, right_alt};
  }
};

inline bool is_typing_key(unsigned int vk, Modifiers mods) {
  if (mods.windows) return false;
  // AltGr layouts use right Alt together with Ctrl. Other Ctrl/Alt combinations
  // are usually shortcuts. Shift is deliberately allowed for capital letters.
  const bool altgr = mods.right_alt && mods.alt && mods.control;
  if ((mods.control || mods.alt) && !altgr) return false;
  return vk == 0x20 ||                              // space
         (vk >= 0x30 && vk <= 0x39) ||             // top-row numbers
         (vk >= 0x41 && vk <= 0x5A) ||             // letters
         (vk >= 0x60 && vk <= 0x6B) ||             // numpad digits, * and +
         (vk >= 0x6D && vk <= 0x6F) ||             // numpad -, . and /
         (vk >= 0xBA && vk <= 0xC2) ||             // punctuation / ABNT
         (vk >= 0xDB && vk <= 0xDF) || vk == 0xE2; // punctuation / ISO key
}

struct Snapshot {
  int wpm = 0;
  std::size_t keys = 0;
  std::array<unsigned int, 15> seconds{}; // oldest to newest; counts only
};

// A fixed-size buffer prevents unbounded growth even with synthetic key bursts.
// It holds only monotonic timestamps for a maximum of 15 seconds. The first
// five seconds use a five-second minimum denominator to avoid huge spikes.
class Meter {
 public:
  static constexpr std::uint64_t window_ms = 15000;
  static constexpr std::uint64_t minimum_ms = 5000;
  static constexpr std::size_t capacity = 4096;

  void clear() {
    timestamps_.fill(0);
    head_ = 0;
    size_ = 0;
    started_ = 0;
    last_time_ = 0;
    has_time_ = false;
  }

  void record(std::uint64_t now) {
    advance(now);
    if (size_ == 0) started_ = now;
    if (size_ == capacity) {
      head_ = (head_ + 1) % capacity;
      --size_;
    }
    timestamps_[(head_ + size_) % capacity] = now;
    ++size_;
  }

  Snapshot snapshot(std::uint64_t now) {
    advance(now);
    Snapshot result;
    result.keys = size_;
    if (size_ == 0) return result;
    const std::uint64_t elapsed = now - started_;
    const std::uint64_t duration = elapsed < minimum_ms ? minimum_ms :
                                   elapsed > window_ms ? window_ms : elapsed;
    // 5 counted keys = one estimated word; result is rounded to nearest WPM.
    result.wpm = static_cast<int>((size_ * 12000ULL + duration / 2) / duration);
    for (std::size_t i = 0; i < size_; ++i) {
      const auto age_seconds = (now - timestamps_[(head_ + i) % capacity]) / 1000;
      if (age_seconds < result.seconds.size()) {
        ++result.seconds[result.seconds.size() - 1 - static_cast<std::size_t>(age_seconds)];
      }
    }
    return result;
  }

 private:
  void advance(std::uint64_t now) {
    // A monotonic clock should not go backwards; recover safely if a test or
    // future clock implementation does, rather than unsigned-underflowing.
    if (has_time_ && now < last_time_) clear();
    has_time_ = true;
    last_time_ = now;
    while (size_ > 0 && now - timestamps_[head_] >= window_ms) {
      timestamps_[head_] = 0;
      head_ = (head_ + 1) % capacity;
      --size_;
    }
    if (size_ == 0) started_ = 0;
  }

  std::array<std::uint64_t, capacity> timestamps_{};
  std::size_t head_ = 0;
  std::size_t size_ = 0;
  std::uint64_t started_ = 0;
  std::uint64_t last_time_ = 0;
  bool has_time_ = false;
};

} // namespace typebloom
