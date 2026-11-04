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

#include "hype-core.hpp"

#include <algorithm>

namespace hype {

namespace {

int clampInt(int value, int low, int high)
{
	if (value < low)
		return low;
	if (value > high)
		return high;
	return value;
}

Config clamped(const Config &config)
{
	Config out = config;

	out.thresholdDb = clampInt(out.thresholdDb, -60, 0);
	out.marginDb = clampInt(out.marginDb, 0, 30);
	out.sustainMs = clampInt(out.sustainMs, 100, 5000);
	out.releaseMs = clampInt(out.releaseMs, 0, 2000);
	out.cooldownMs = clampInt(out.cooldownMs, 0, 600000);
	out.baselineMs = clampInt(out.baselineMs, 1000, 120000);

	return out;
}

} // namespace

void Detector::setConfig(const Config &config)
{
	config_ = clamped(config);
}

bool Detector::hypeCondition() const
{
	if (!anyMeterAlive_)
		return false;

	if (db_ < (float)config_.thresholdDb)
		return false;

	/* A stream that is loud all the time lifts the baseline until this
	 * stops being true, which is exactly what we want. */
	return db_ >= baselineDb_ + (float)config_.marginDb;
}

bool Detector::coolDownActive(int64_t nowMs) const
{
	return nowMs < cooldownUntilMs_;
}

void Detector::submit(int64_t nowMs, const std::vector<Level> &levels)
{
	float loudest = -100.0f;
	int alive = 0;

	for (const Level &level : levels) {
		if (!level.alive)
			continue;

		alive++;
		if (level.db > loudest)
			loudest = level.db;
	}

	anyMeterAlive_ = alive > 0;
	db_ = anyMeterAlive_ ? loudest : -100.0f;

	/* The baseline follows the room with a time constant, so it is learned
	 * rather than configured. Samples are weighted by the time since the
	 * previous one: a plugin that polls twice as often does not adapt twice
	 * as fast. */
	if (anyMeterAlive_) {
		/* The baseline starts at -60 dB rather than at the first sample:
		 * the opening shout of a stream has no history to stand out from
		 * and is usually the one worth clipping. */
		haveBaseline_ = true;

		if (lastSubmitMs_ > 0 && nowMs > lastSubmitMs_) {
			const double dt = (double)(nowMs - lastSubmitMs_);
			const double alpha = std::min(1.0, dt / (double)config_.baselineMs);
			baselineDb_ = (float)((double)baselineDb_ + ((double)db_ - (double)baselineDb_) * alpha);
		}
	}

	lastSubmitMs_ = nowMs;
}

Request Detector::tick(int64_t nowMs)
{
	Request request;

	if (!config_.enabled || !config_.autoClip)
		return request;

	const bool loud = hypeCondition();

	if (loud) {
		if (streakStartMs_ < 0) {
			streakStartMs_ = nowMs;
			peakDb_ = db_;
			requested_ = false;
		} else {
			peakDb_ = std::max(peakDb_, db_);
		}

		lastLoudMs_ = nowMs;
	} else if (streakStartMs_ >= 0) {
		/* A short drop-out (a breath, a beat) keeps the streak alive; a
		 * longer one ends it. */
		if (lastLoudMs_ >= 0 && nowMs - lastLoudMs_ > (int64_t)config_.releaseMs)
			streakStartMs_ = -1;
	}

	if (streakStartMs_ < 0 || requested_)
		return request;

	if (nowMs - streakStartMs_ < (int64_t)config_.sustainMs)
		return request;

	if (coolDownActive(nowMs))
		return request;

	requested_ = true;
	request.wanted = true;
	request.scoreDb = peakDb_;
	request.sinceMs = streakStartMs_;

	return request;
}

void Detector::noteClip(int64_t nowMs, float scoreDb)
{
	cooldownUntilMs_ = nowMs + (int64_t)config_.cooldownMs;

	/* A manual clip means the moment is already in the bag: do not follow
	 * it with an automatic one. */
	requested_ = true;
	peakDb_ = std::max(peakDb_, scoreDb);
}

void Detector::reset(int64_t nowMs)
{
	db_ = -100.0f;
	peakDb_ = -100.0f;
	baselineDb_ = kBaselineStartDb;
	haveBaseline_ = false;
	anyMeterAlive_ = false;
	lastSubmitMs_ = nowMs;
	streakStartMs_ = -1;
	lastLoudMs_ = -1;
	cooldownUntilMs_ = 0;
	requested_ = false;
}

Status Detector::status(int64_t nowMs) const
{
	Status status;

	status.db = db_;
	status.baselineDb = haveBaseline_ ? baselineDb_ : kBaselineStartDb;
	status.peakDb = peakDb_;
	status.anyMeterAlive = anyMeterAlive_;
	status.loudNow = hypeCondition();
	status.loudForMs = streakStartMs_ >= 0 ? nowMs - streakStartMs_ : 0;
	status.cooldownLeftMs = coolDownActive(nowMs) ? cooldownUntilMs_ - nowMs : 0;

	return status;
}

} // namespace hype
