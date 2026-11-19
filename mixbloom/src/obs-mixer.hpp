/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
Licensed under GPL-2.0-or-later; see LICENSE. No warranty.
*/
#pragma once

#include "mix-core.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mixbloom {

enum class Result { Applied, Missing, Locked, Unsupported, Invalid, ConfirmClearTracks };

/* Reads every public audio-capable input in the current OBS collection.
 * None of these functions creates a source, changes settings on load, or keeps
 * a strong source reference after it returns. */
std::vector<Source> readSources();
Result setFader(const std::string &uuid, int position);
Result setMuted(const std::string &uuid, bool muted);
Result setTrack(const std::string &uuid, unsigned track, bool enabled, bool allowClearAll = false);
Result setMonitoring(const std::string &uuid, bool enabled);

/* Retains only a weak identity across a confirmation dialog. If a source is
 * removed/replaced while the dialog is open, the pending action is cancelled. */
class SourceIdentity {
public:
	explicit SourceIdentity(const std::string &uuid);
	~SourceIdentity();
	SourceIdentity(const SourceIdentity &) = delete;
	SourceIdentity &operator=(const SourceIdentity &) = delete;
	bool stillCurrent() const;
private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

struct PeakSample {
	float db;
	uint64_t sequence;
};

/* OBS invokes the meter on an audio thread. It writes only atomics; the GUI
 * samples them on its own thread. Removal waits for any callback to finish. */
class MeterTap {
public:
	explicit MeterTap(const std::string &uuid);
	~MeterTap();
	MeterTap(const MeterTap &) = delete;
	MeterTap &operator=(const MeterTap &) = delete;
	bool attached() const;
	PeakSample sample() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace mixbloom
