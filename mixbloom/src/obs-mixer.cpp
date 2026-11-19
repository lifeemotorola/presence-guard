/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
Licensed under GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include "obs-mixer.hpp"

#include <obs-module.h>
#include <obs-audio-controls.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace mixbloom {
namespace {

struct SourceRelease {
	void operator()(obs_source_t *source) const { obs_source_release(source); }
};
using SourceRef = std::unique_ptr<obs_source_t, SourceRelease>;

bool isAudio(obs_source_t *source)
{
	return source && !obs_source_removed(source) && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0 &&
	       (obs_source_get_output_flags(source) & OBS_SOURCE_SUBMIX) == 0;
}

SourceRef findAudio(const std::string &uuid)
{
	if (uuid.empty())
		return SourceRef(nullptr);
	SourceRef source(obs_get_source_by_uuid(uuid.c_str()));
	if (!isAudio(source.get()))
		source.reset();
	return source;
}

bool isVolumeLocked(obs_source_t *source)
{
	obs_data_t *settings = obs_source_get_private_settings(source);
	const bool locked = settings && obs_data_get_bool(settings, "volume_locked");
	obs_data_release(settings);
	return locked;
}

bool collect(void *param, obs_source_t *source)
{
	if (!isAudio(source))
		return true;
	const char *uuid = obs_source_get_uuid(source);
	if (!uuid || !*uuid)
		return true;
	const char *name = obs_source_get_name(source);
	const uint32_t flags = obs_source_get_output_flags(source);
	Source item;
	item.uuid = uuid;
	item.name = name && *name ? name : "(unnamed audio source)";
	item.gain = obs_source_get_volume(source);
	item.muted = obs_source_muted(source);
	item.tracks = obs_source_get_audio_mixers(source);
	item.monitoring = obs_source_get_monitoring_enabled(source);
	item.canMonitor = (flags & OBS_SOURCE_DO_NOT_SELF_MONITOR) == 0;
	item.volumeLocked = isVolumeLocked(source);
	static_cast<std::vector<Source> *>(param)->push_back(std::move(item));
	return true;
}

} // namespace

std::vector<Source> readSources()
{
	std::vector<Source> sources;
	obs_enum_sources(collect, &sources);
	/* Stable identity comes from UUID, never from a possibly duplicated name. */
	std::sort(sources.begin(), sources.end(), [](const Source &a, const Source &b) {
		return a.name == b.name ? a.uuid < b.uuid : a.name < b.name;
	});
	return sources;
}

Result setFader(const std::string &uuid, int position)
{
	if (position < 0 || position > kFaderMax)
		return Result::Invalid;
	auto source = findAudio(uuid);
	if (!source)
		return Result::Missing;
	if (isVolumeLocked(source.get()))
		return Result::Locked;
	obs_source_set_volume(source.get(), faderToGain(position));
	return Result::Applied;
}

Result setMuted(const std::string &uuid, bool muted)
{
	auto source = findAudio(uuid);
	if (!source)
		return Result::Missing;
	obs_source_set_muted(source.get(), muted);
	return Result::Applied;
}

Result setTrack(const std::string &uuid, unsigned track, bool enabled, bool allowClearAll)
{
	if (track < 1 || track > kTrackCount)
		return Result::Invalid;
	auto source = findAudio(uuid);
	if (!source)
		return Result::Missing;
	/* Re-read immediately before the write: don't overwrite changes to other
	 * tracks made in OBS after the last UI refresh. */
	const uint32_t before = obs_source_get_audio_mixers(source.get());
	const uint32_t after = withTrack(before, track, enabled);
	if (!enabled && (before & kTrackMask) != 0 && (after & kTrackMask) == 0 && !allowClearAll)
		return Result::ConfirmClearTracks;
	if (after != before)
		obs_source_set_audio_mixers(source.get(), after);
	return Result::Applied;
}

Result setMonitoring(const std::string &uuid, bool enabled)
{
	auto source = findAudio(uuid);
	if (!source)
		return Result::Missing;
	/* OBS marks desktop/output captures that could feed back. Never turn
	 * monitoring on for such a source; turning existing monitoring off is OK. */
	if (enabled && (obs_source_get_output_flags(source.get()) & OBS_SOURCE_DO_NOT_SELF_MONITOR))
		return Result::Unsupported;
	obs_source_set_monitoring_enabled(source.get(), enabled);
	return Result::Applied;
}

struct SourceIdentity::Impl {
	std::string uuid;
	obs_weak_source_t *source = nullptr;
};

SourceIdentity::SourceIdentity(const std::string &uuid) : impl_(std::make_unique<Impl>())
{
	impl_->uuid = uuid;
	auto source = findAudio(uuid);
	if (source)
		impl_->source = obs_source_get_weak_source(source.get());
}

SourceIdentity::~SourceIdentity()
{
	if (impl_->source)
		obs_weak_source_release(impl_->source);
}

bool SourceIdentity::stillCurrent() const
{
	if (!impl_->source)
		return false;
	SourceRef original(obs_weak_source_get_source(impl_->source));
	SourceRef current = findAudio(impl_->uuid);
	return original && current && original.get() == current.get();
}

struct MeterTap::Impl {
	obs_volmeter_t *meter = nullptr;
	obs_weak_source_t *source = nullptr;
	std::string uuid;
	std::atomic<float> peak{-std::numeric_limits<float>::infinity()};
	std::atomic<uint64_t> sequence{0};

	static void update(void *context, const float magnitude[MAX_AUDIO_CHANNELS],
			   const float peaks[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
	{
		(void)magnitude;
		(void)inputPeak;
		auto *self = static_cast<Impl *>(context);
		float highest = -std::numeric_limits<float>::infinity();
		for (int channel = 0; channel < MAX_AUDIO_CHANNELS; ++channel) {
			if (!std::isnan(peaks[channel]) && peaks[channel] > highest)
				highest = peaks[channel];
		}
		self->peak.store(highest, std::memory_order_relaxed);
		self->sequence.fetch_add(1, std::memory_order_release);
	}
};

MeterTap::MeterTap(const std::string &uuid) : impl_(std::make_unique<Impl>())
{
	impl_->uuid = uuid;
	auto source = findAudio(uuid);
	if (!source)
		return;
	impl_->source = obs_source_get_weak_source(source.get());
	if (!impl_->source)
		return;
	impl_->meter = obs_volmeter_create(OBS_FADER_CUBIC);
	if (!impl_->meter)
		return;
	if (!obs_volmeter_attach_source(impl_->meter, source.get())) {
		obs_volmeter_destroy(impl_->meter);
		impl_->meter = nullptr;
		return;
	}
	obs_volmeter_add_callback(impl_->meter, &Impl::update, impl_.get());
}

MeterTap::~MeterTap()
{
	/* Temporarily pin the original source while OBS disconnects the meter.
	 * The dock does not hold strong references between refreshes. */
	SourceRef source(impl_->source ? obs_weak_source_get_source(impl_->source) : nullptr);
	if (impl_->meter) {
		/* OBS serializes callback invocation/removal with its callback mutex. */
		obs_volmeter_remove_callback(impl_->meter, &Impl::update, impl_.get());
		obs_volmeter_destroy(impl_->meter);
	}
	if (impl_->source)
		obs_weak_source_release(impl_->source);
}

bool MeterTap::attached() const
{
	if (!impl_->meter || !impl_->source)
		return false;
	/* A source can be removed and recreated with the same UUID between two
	 * refreshes. Do not keep displaying a meter attached to the old object. */
	SourceRef previous(obs_weak_source_get_source(impl_->source));
	SourceRef current = findAudio(impl_->uuid);
	return previous && current && previous.get() == current.get();
}

PeakSample MeterTap::sample() const
{
	const uint64_t sequence = impl_->sequence.load(std::memory_order_acquire);
	return {impl_->peak.load(std::memory_order_relaxed), sequence};
}

} // namespace mixbloom
