/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
Licensed under GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include "mix-core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mixbloom {

float faderToGain(int position)
{
	const float x = static_cast<float>(std::clamp(position, 0, kFaderMax)) / kFaderMax;
	return x * x * x;
}

int gainToFader(float gain)
{
	if (std::isnan(gain) || gain <= 0.0f)
		return 0;
	if (gain >= 1.0f)
		return kFaderMax;
	return static_cast<int>(std::lround(std::cbrt(gain) * kFaderMax));
}

float gainToDb(float gain)
{
	if (std::isnan(gain) || gain <= 0.0f)
		return -std::numeric_limits<float>::infinity();
	return 20.0f * std::log10(gain);
}

int peakToPercent(float db)
{
	if (std::isnan(db) || db <= -60.0f)
		return 0;
	if (db >= 0.0f)
		return 100;
	return static_cast<int>(std::lround((db + 60.0f) * (100.0f / 60.0f)));
}

uint32_t withTrack(uint32_t mask, unsigned track, bool enabled)
{
	if (track < 1 || track > kTrackCount)
		return mask;
	const uint32_t bit = 1u << (track - 1);
	return enabled ? mask | bit : mask & ~bit;
}

bool trackEnabled(uint32_t mask, unsigned track)
{
	return track >= 1 && track <= kTrackCount && (mask & (1u << (track - 1))) != 0;
}

} // namespace mixbloom
