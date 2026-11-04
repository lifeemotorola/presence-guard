/*
Auto Director for OBS Studio
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

namespace director {

/*
 * One "take" is a camera: the scene the director cuts to, plus the audio
 * source that belongs to whoever is on that camera. The level of that source
 * is the only thing that decides who is on air.
 */
struct Take {
	std::string scene;
	std::string source;
	bool enabled = true;

	bool usable() const { return enabled && !scene.empty(); }
};

/* A single meter reading for a take, taken on the Qt main thread. */
struct Level {
	float db = -100.0f;
	/* False when the meter stopped producing values: muted, not in the live
	 * scene, or the device went away. Such a take can never count as
	 * speaking, and its silence is never trusted either. */
	bool alive = false;
};

struct Config {
	bool enabled = false;
	bool onlyWhileLive = true;

	/* A take only speaks above this level. */
	int thresholdDb = -45;
	/* Another take must beat the one on air by this much to steal the shot. */
	int hysteresisDb = 3;
	/* How long a new speaker must keep talking before automatic switching
	 * cuts to them. This is what stops a cough, a laugh or a door slam from
	 * stealing the shot: it also caps how often automatic cuts can happen.
	 * Zero cuts as soon as somebody is clearly speaking. */
	int dwellMs = 4000;
	/* Stay on the fallback scene (lobby/wide) after this much silence.
	 * Zero or an empty fallback scene disables the rule. */
	int fallbackDelaySec = 30;
	std::string fallbackScene;
	/* How long a manual scene change suspends the director. */
	int operatorLockMs = 15000;
};

enum class Action { None, Switch };

struct Decision {
	Action action = Action::None;
	std::string scene;
	std::string reason;
};

/*
 * The whole directing policy lives here, with no libobs and no Qt, so it can be
 * driven by tests at whatever time steps they like.
 */
class Core {
public:
	void setConfig(const Config &config);
	const Config &config() const { return config_; }

	void setTakes(std::vector<Take> takes);
	const std::vector<Take> &takes() const { return takes_; }

	void submitLevels(int64_t nowMs, const std::vector<Level> &levels);

	/*
	 * One directing pass. `currentScene` is what is live right now;
	 * `live` means streaming or recording.
	 */
	Decision tick(int64_t nowMs, const std::string &currentScene, bool live, bool studioMode);

	/* Suspends automatic switching for config().operatorLockMs. */
	void lock(int64_t nowMs);
	void unlock();
	bool locked(int64_t nowMs) const;
	int64_t lockedForMs(int64_t nowMs) const;

	/* Operator pressed "next": take the next usable take, ignoring dwell. */
	void forceNext();

	/* The operator changed scene themselves - back off instead of fighting. */
	void noteOperatorChange(int64_t nowMs);

	/* Silence since somebody last spoke, for the status line. */
	int64_t quietMs(int64_t nowMs) const;

	bool anyMeterAlive() const { return metersAlive_ > 0; }

private:
	const Level &levelAt(size_t index) const;
	bool speakingAt(size_t index) const;
	int bestSpeakingIndex() const;
	int indexOfScene(const std::string &scene) const;
	void restartTimers(int64_t nowMs);

	Config config_;
	std::vector<Take> takes_;
	std::vector<Level> levels_;
	int metersAlive_ = 0;

	int64_t lastSpeechMs_ = 0;
	int64_t lockUntilMs_ = 0;
	/* Since when the current best speaker has been the best speaker. */
	int64_t bestSinceMs_ = 0;
	int bestSinceIndex_ = -1;
	bool started_ = false;
	bool forceNextPending_ = false;
};

} // namespace director
