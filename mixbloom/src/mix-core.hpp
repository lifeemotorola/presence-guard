/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#pragma once

#include <cstdint>
#include <string>

namespace mixbloom {

constexpr int kFaderMax = 1000;
constexpr unsigned kTrackCount = 6;
constexpr uint32_t kTrackMask = (1u << kTrackCount) - 1u;

struct Source {
	std::string uuid;
	std::string name;
	float gain = 0.0f;
	uint32_t tracks = 0;
	bool muted = false;
	bool monitoring = false;
	bool canMonitor = false;
	bool volumeLocked = false;
};

/* An OBS-style cubic fader. MixBloom never amplifies beyond unity (0 dB).
 * An externally configured gain above unity is displayed but not changed
 * until the user actually moves the fader. */
float faderToGain(int position);
int gainToFader(float gain);
float gainToDb(float gain);
int peakToPercent(float db);

/* Tracks are numbered 1..6 in the UI. Preserve all other mask bits. */
uint32_t withTrack(uint32_t mask, unsigned track, bool enabled);
bool trackEnabled(uint32_t mask, unsigned track);

} // namespace mixbloom
