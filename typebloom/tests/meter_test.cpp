/* Copyright (C) 2026 Emmanuel Suah. SPDX-License-Identifier: GPL-2.0-or-later */
#include "meter.h"
#include <cstdlib>
#include <iostream>
#include <numeric>

using namespace typebloom;

static int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition " failed\n"; std::exit(1); \
} } while (0)

int main() {
  for (unsigned int vk = 0; vk <= 255; ++vk) {
    CHECK(!is_typing_key(vk, {true, false, false, false}));
    CHECK(!is_typing_key(vk, {false, true, false, false}));
    CHECK(!is_typing_key(vk, {false, false, true, false}));
  }
  for (unsigned int vk : {0x20, 0x30, 0x39, 0x41, 0x5A, 0x60, 0x69, 0x6A, 0x6B,
                          0x6D, 0x6E, 0x6F, 0xBA, 0xC0, 0xC1, 0xC2,
                          0xDB, 0xDF, 0xE2}) {
    CHECK(is_typing_key(vk, {}));
    CHECK(is_typing_key(vk, {true, true, false, true})); // AltGr
    CHECK(!is_typing_key(vk, {true, true, true, true}));
  }
  for (unsigned int vk : {0x08, 0x09, 0x0D, 0x1B, 0x21, 0x25, 0x2E, 0x70,
                          0x7B, 0x10, 0x11, 0x12, 0x5B, 0x6C, 0xE5, 0xE7}) {
    CHECK(!is_typing_key(vk, {}));
  }

  ModifierState modifiers;
  CHECK(is_typing_key(0x41, modifiers.current()));
  modifiers.observe(0xA2, true); // left Ctrl down
  CHECK(!is_typing_key(0x41, modifiers.current()));
  modifiers.observe(0xA5, true); // right Alt down with Ctrl = AltGr
  CHECK(is_typing_key(0x41, modifiers.current()));
  modifiers.observe(0x5B, true); // Windows still blocks
  CHECK(!is_typing_key(0x41, modifiers.current()));
  modifiers.observe(0x5B, false);
  modifiers.observe(0xA5, false);
  CHECK(!is_typing_key(0x41, modifiers.current()));
  modifiers.observe(0xA2, false);
  CHECK(is_typing_key(0x41, modifiers.current()));
  modifiers.observe(0x12, true); // generic Alt fallback
  CHECK(!is_typing_key(0x41, modifiers.current()));
  modifiers.observe(0x12, false);
  CHECK(is_typing_key(0x41, modifiers.current()));

  Meter meter;
  auto s = meter.snapshot(1000);
  CHECK(s.keys == 0 && s.wpm == 0);
  CHECK(std::accumulate(s.seconds.begin(), s.seconds.end(), 0U) == 0);
  meter.record(1000);
  s = meter.snapshot(1000);
  CHECK(s.keys == 1 && s.wpm == 2); // 1 / 5 words in 5 seconds = 2.4 WPM
  CHECK(s.seconds[14] == 1);
  for (int i = 1; i < 50; ++i) meter.record(1000 + i * 200);
  s = meter.snapshot(11000);
  CHECK(s.keys == 50 && s.wpm == 60); // 50 keys / 5 / (10 / 60)
  CHECK(std::accumulate(s.seconds.begin(), s.seconds.end(), 0U) == 50);
  CHECK(s.seconds[4] == 1 && s.seconds[14] == 4);
  s = meter.snapshot(16000);
  CHECK(s.keys == 49 && s.wpm == 39); // the first key expires exactly at 15s
  s = meter.snapshot(26000);
  CHECK(s.keys == 0 && s.wpm == 0);
  meter.record(26000);
  CHECK(meter.snapshot(26000).wpm == 2); // idle begins a fresh warm-up
  meter.clear();
  CHECK(meter.snapshot(26000).keys == 0);

  meter.record(200);
  meter.record(100); // recover from a backwards clock without underflow
  CHECK(meter.snapshot(100).keys == 1);
  CHECK(meter.snapshot(99).keys == 0);
  for (std::size_t i = 0; i < Meter::capacity + 123; ++i) meter.record(500);
  s = meter.snapshot(500);
  CHECK(s.keys == Meter::capacity);
  CHECK(s.seconds[14] == Meter::capacity);
  CHECK(meter.snapshot(15500).keys == 0);
  std::cout << "TypeBloom meter: " << assertions << " checks passed\n";
}
