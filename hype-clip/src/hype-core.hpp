/*
Hype Clip for OBS Studio
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

/*
 * Deciding when something clip-worthy happened is the whole trick of this
 * plugin, and it is pure logic: no libobs, no Qt, no clock of its own. Feed it
 * meter readings and the current time and it answers one question - "should we
 * clip that?".
 *
 * The rule has three parts, and all of them have to agree:
 *
 *   1. it has to be loud in absolute terms (thresholdDb),
 *   2. it has to stand out from the room the stream has been in so far
 *      (the running baseline plus marginDb),
 *   3. it has to stay that way for a moment (sustainMs), because a single
 *      loud sample is a cough, a door or a Discord notification.
 *
 * A cooldown keeps the plugin from turning one long scream into ten clips, and
 * the baseline keeps adapting, so a stream that is loud all the time does not
 * clip itself every twenty seconds forever.
 */
namespace hype {

struct Level {
	float db = -100.0f;
	/* False when the source stopped producing audio (muted, unplugged).
	 * Such a meter is never read as silence and never as a shout. */
	bool alive = false;
};

struct Config {
	bool enabled = true;
	bool autoClip = true;

	/* Absolute loudness that can count as a moment. */
	int thresholdDb = -12;
	/* How far above the running baseline it has to be. */
	int marginDb = 6;
	/* How long it has to stay that loud. */
	int sustainMs = 700;
	/* A drop-out shorter than this does not break the streak. */
	int releaseMs = 300;
	/* Minimum gap between two automatic clips. */
	int cooldownMs = 20000;
	/* Time constant of the running baseline. */
	int baselineMs = 15000;
};

/* Everything the dock needs to draw a live meter next to the rules. */
struct Status {
	float db = -100.0f;
	float baselineDb = -60.0f;
	float peakDb = -100.0f;
	bool anyMeterAlive = false;
	bool loudNow = false;
	int64_t loudForMs = 0;
	int64_t cooldownLeftMs = 0;
};

/* What tick() decided. */
struct Request {
	bool wanted = false;
	/* The loudest sample of the streak, used as the "how hype was it" score. */
	float scoreDb = -100.0f;
	/* When the streak started, for the reason line. */
	int64_t sinceMs = 0;
};

class Detector {
public:
	/* The baseline starts low: the first loud moment of a stream has no
	 * history to stand out from, and it is usually the one worth clipping. */
	static constexpr float kBaselineStartDb = -60.0f;

	void setConfig(const Config &config);
	const Config &config() const { return config_; }

	/* One round of meter readings, taken every poll interval. */
	void submit(int64_t nowMs, const std::vector<Level> &levels);

	/* One decision pass. Returns a request at most once per streak. */
	Request tick(int64_t nowMs);

	/* A clip was saved - automatic or manual. Starts the cooldown. */
	void noteClip(int64_t nowMs, float scoreDb);

	/* Forget the streak, the baseline and the cooldown. */
	void reset(int64_t nowMs);

	Status status(int64_t nowMs) const;

private:
	bool coolDownActive(int64_t nowMs) const;
	bool hypeCondition() const;

	Config config_;

	float db_ = -100.0f;
	float peakDb_ = -100.0f;
	float baselineDb_ = kBaselineStartDb;
	bool anyMeterAlive_ = false;
	bool haveBaseline_ = false;

	int64_t lastSubmitMs_ = 0;
	int64_t streakStartMs_ = -1;
	int64_t lastLoudMs_ = -1;
	int64_t cooldownUntilMs_ = 0;
	bool requested_ = false;
};

} // namespace hype
