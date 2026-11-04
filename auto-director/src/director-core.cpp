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

#include "director-core.hpp"

namespace director {

void Core::setConfig(const Config &config)
{
	config_ = config;
}

void Core::setTakes(std::vector<Take> takes)
{
	takes_ = std::move(takes);
	levels_.assign(takes_.size(), Level());
	metersAlive_ = 0;

	/* A rebuilt take list must not inherit a stale silence clock, or the
	 * first tick after the change would dive straight to the lobby. */
	started_ = false;
}

void Core::submitLevels(int64_t nowMs, const std::vector<Level> &levels)
{
	(void)nowMs;

	levels_.assign(takes_.size(), Level());

	metersAlive_ = 0;
	for (size_t i = 0; i < levels.size() && i < levels_.size(); i++) {
		levels_[i] = levels[i];
		if (levels_[i].alive)
			metersAlive_++;
	}
}

const Level &Core::levelAt(size_t index) const
{
	static const Level kMissing;
	if (index >= levels_.size())
		return kMissing;
	return levels_[index];
}

bool Core::speakingAt(size_t index) const
{
	const Level &level = levelAt(index);
	return level.alive && level.db >= (float)config_.thresholdDb;
}

int Core::bestSpeakingIndex() const
{
	int best = -1;

	for (size_t i = 0; i < takes_.size(); i++) {
		if (!takes_[i].usable() || !speakingAt(i))
			continue;

		/* Loudest wins; ties go to the higher take - the operator's order. */
		if (best < 0 || levelAt(i).db > levelAt((size_t)best).db)
			best = (int)i;
	}

	return best;
}

int Core::indexOfScene(const std::string &scene) const
{
	if (scene.empty())
		return -1;

	for (size_t i = 0; i < takes_.size(); i++) {
		if (takes_[i].scene == scene)
			return (int)i;
	}

	return -1;
}

void Core::restartTimers(int64_t nowMs)
{
	lastSpeechMs_ = nowMs;
	bestSinceMs_ = nowMs;
	bestSinceIndex_ = -1;
	started_ = false;
	forceNextPending_ = false;
}

void Core::lock(int64_t nowMs)
{
	lockUntilMs_ = nowMs + (int64_t)config_.operatorLockMs;
}

void Core::unlock()
{
	lockUntilMs_ = 0;
}

bool Core::locked(int64_t nowMs) const
{
	return lockUntilMs_ > nowMs;
}

int64_t Core::lockedForMs(int64_t nowMs) const
{
	const int64_t remaining = lockUntilMs_ - nowMs;
	return remaining > 0 ? remaining : 0;
}

void Core::forceNext()
{
	forceNextPending_ = true;
	unlock();
}

void Core::noteOperatorChange(int64_t nowMs)
{
	/* Whoever the operator just put on air gets a clean slate: no pending
	 * dwell, no pending manual advance, and the lock on top. */
	bestSinceMs_ = nowMs;
	bestSinceIndex_ = -1;
	forceNextPending_ = false;
	lock(nowMs);
}

int64_t Core::quietMs(int64_t nowMs) const
{
	const int64_t quiet = nowMs - lastSpeechMs_;
	return quiet > 0 ? quiet : 0;
}

Decision Core::tick(int64_t nowMs, const std::string &currentScene, bool live, bool studioMode)
{
	Decision none;

	if (!config_.enabled || (config_.onlyWhileLive && !live)) {
		/* Start every session with a full dwell and a clean silence count,
		 * so switching the director on never cuts on stale bookkeeping. */
		restartTimers(nowMs);
		return none;
	}

	if (!started_) {
		started_ = true;
		lastSpeechMs_ = nowMs;
		bestSinceMs_ = nowMs;
		bestSinceIndex_ = -1;
	}

	/* Windowed preview/program: the operator is clearly at the controls. */
	if (studioMode) {
		forceNextPending_ = false;
		return none;
	}

	if (forceNextPending_) {
		forceNextPending_ = false;

		const int current = indexOfScene(currentScene);
		for (size_t step = 1; step <= takes_.size(); step++) {
			const size_t candidate = ((current < 0 ? 0 : (size_t)current) + step) % takes_.size();
			if (!takes_[candidate].usable())
				continue;

			lastSpeechMs_ = nowMs;
			bestSinceMs_ = nowMs;
			bestSinceIndex_ = -1;
			return {Action::Switch, takes_[candidate].scene, "manual: next take"};
		}

		return none;
	}

	const int best = bestSpeakingIndex();

	if (best >= 0) {
		lastSpeechMs_ = nowMs;

		/* Track how long this take has been the loudest one; a new voice
		 * starts its dwell from scratch. */
		if (best != bestSinceIndex_) {
			bestSinceIndex_ = best;
			bestSinceMs_ = nowMs;
		}
	} else {
		bestSinceIndex_ = -1;
	}

	/* The operator is driving; keep the silence clock honest but do not
	 * cut, and start a fresh dwell once control comes back. */
	if (locked(nowMs)) {
		bestSinceMs_ = nowMs;
		bestSinceIndex_ = -1;
		return none;
	}

	if (best >= 0) {
		const std::string &bestScene = takes_[(size_t)best].scene;

		if (bestScene == currentScene)
			return none;

		const int current = indexOfScene(currentScene);
		const bool currentSpeaking = current >= 0 && speakingAt((size_t)current);
		const bool dwellElapsed = (nowMs - bestSinceMs_) >= (int64_t)config_.dwellMs;

		float margin = 0.0f;
		if (currentSpeaking)
			margin = levelAt((size_t)best).db - levelAt((size_t)current).db;

		const bool beatsCurrent = !currentSpeaking || margin >= (float)config_.hysteresisDb;

		if (!dwellElapsed || !beatsCurrent)
			return none;

		std::string reason = "speaker on '" + takes_[(size_t)best].source + "'";
		if (currentSpeaking) {
			reason += " (+" + std::to_string((int)(margin + 0.5f)) + " dB over the current take)";
		} else if (current >= 0) {
			reason += " (previous take went quiet)";
		}

		return {Action::Switch, bestScene, reason};
	}

	/* Nobody is speaking. Falling back to a lobby scene is only meaningful
	 * when at least one meter is actually reporting - otherwise "silence"
	 * may just be a muted or missing device, which must not black the
	 * stream out to a BRB scene. */
	const bool fallbackConfigured = !config_.fallbackScene.empty() && config_.fallbackDelaySec > 0;

	if (!fallbackConfigured || metersAlive_ == 0)
		return none;

	if (currentScene == config_.fallbackScene)
		return none;

	if (quietMs(nowMs) < (int64_t)config_.fallbackDelaySec * 1000)
		return none;

	return {Action::Switch, config_.fallbackScene,
		"no speech for " + std::to_string(config_.fallbackDelaySec) + " s"};
}

} // namespace director
