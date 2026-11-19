/*
Test-only OBS simulation for MixBloom.
Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later; no warranty.
*/
#include "fake-obs.hpp"
#include <obs-module.h>
#include <obs-audio-controls.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct obs_source : FakeSource {};
struct obs_weak_source { obs_source_t *source = nullptr; };
struct obs_data { bool locked = false; };
struct obs_volmeter {
	obs_source_t *source = nullptr;
	obs_volmeter_updated_t callback = nullptr;
	void *context = nullptr;
	std::mutex callbackMutex;
};

static std::vector<std::unique_ptr<obs_source_t>> sources;
static std::vector<obs_volmeter_t *> meters;
static std::mutex metersMutex;
static int writes = 0, acquired = 0, released = 0;

void fakeReset()
{
	/* All test-owned taps/docks must have been destroyed first. */
	std::lock_guard<std::mutex> lock(metersMutex);
	if (!meters.empty())
		std::terminate();
	sources.clear();
	writes = acquired = released = 0;
}

FakeSource &fakeAdd(const std::string &uuid, const std::string &name, uint32_t flags)
{
	auto source = std::make_unique<obs_source_t>();
	source->uuid = uuid;
	source->name = name;
	source->flags = flags;
	sources.push_back(std::move(source));
	return *sources.back();
}
FakeSource *fakeFind(const std::string &uuid)
{
	for (const auto &source : sources)
		if (source->uuid == uuid)
			return source.get();
	return nullptr;
}
int fakeWrites() { return writes; }
int fakeAcquired() { return acquired; }
int fakeReleased() { return released; }
int fakeMeters() { std::lock_guard<std::mutex> lock(metersMutex); return static_cast<int>(meters.size()); }

void obs_enum_sources(bool (*callback)(void *, obs_source_t *), void *context)
{
	for (const auto &source : sources)
		if (!callback(context, source.get()))
			break;
}
obs_source_t *obs_get_source_by_uuid(const char *id)
{
	for (const auto &source : sources) {
		if (source->uuid == id && !source->removed) {
			++source->refs;
			++acquired;
			return source.get();
		}
	}
	return nullptr;
}
void obs_source_release(obs_source_t *source)
{
	if (source) { --source->refs; ++released; }
}
obs_weak_source_t *obs_source_get_weak_source(obs_source_t *source)
{
	return new obs_weak_source_t{source};
}
obs_source_t *obs_weak_source_get_source(obs_weak_source_t *weak)
{
	if (!weak || !weak->source || weak->source->removed)
		return nullptr;
	++weak->source->refs;
	++acquired;
	return weak->source;
}
void obs_weak_source_release(obs_weak_source_t *weak) { delete weak; }
bool obs_source_removed(const obs_source_t *s) { return s->removed; }
uint32_t obs_source_get_output_flags(const obs_source_t *s) { return s->flags; }
const char *obs_source_get_uuid(const obs_source_t *s) { return s->uuid.c_str(); }
const char *obs_source_get_name(const obs_source_t *s) { return s->name.c_str(); }
float obs_source_get_volume(const obs_source_t *s) { return s->gain; }
void obs_source_set_volume(obs_source_t *s, float gain) { s->gain = gain; ++writes; }
bool obs_source_muted(const obs_source_t *s) { return s->muted; }
void obs_source_set_muted(obs_source_t *s, bool value) { s->muted = value; ++writes; }
uint32_t obs_source_get_audio_mixers(const obs_source_t *s) { return s->tracks; }
void obs_source_set_audio_mixers(obs_source_t *s, uint32_t value) { s->tracks = value; ++writes; }
bool obs_source_get_monitoring_enabled(const obs_source_t *s) { return s->monitoring; }
void obs_source_set_monitoring_enabled(obs_source_t *s, bool value) { s->monitoring = value; ++writes; }
obs_data_t *obs_source_get_private_settings(obs_source_t *s) { return new obs_data_t{s->locked}; }
bool obs_data_get_bool(const obs_data_t *data, const char *) { return data->locked; }
void obs_data_release(obs_data_t *data) { delete data; }

obs_volmeter_t *obs_volmeter_create(enum obs_fader_type)
{
	auto *meter = new obs_volmeter_t;
	std::lock_guard<std::mutex> lock(metersMutex);
	meters.push_back(meter);
	return meter;
}
void obs_volmeter_destroy(obs_volmeter_t *meter)
{
	std::lock_guard<std::mutex> lock(metersMutex);
	meters.erase(std::remove(meters.begin(), meters.end(), meter), meters.end());
	delete meter;
}
bool obs_volmeter_attach_source(obs_volmeter_t *meter, obs_source_t *source)
{
	meter->source = source;
	return true;
}
void obs_volmeter_add_callback(obs_volmeter_t *meter, obs_volmeter_updated_t callback, void *context)
{
	std::lock_guard<std::mutex> lock(meter->callbackMutex);
	meter->callback = callback;
	meter->context = context;
}
void obs_volmeter_remove_callback(obs_volmeter_t *meter, obs_volmeter_updated_t callback, void *context)
{
	std::lock_guard<std::mutex> lock(meter->callbackMutex);
	if (meter->callback == callback && meter->context == context) {
		meter->callback = nullptr;
		meter->context = nullptr;
	}
}
void fakeEmitPeak(const std::string &uuid, float leftDb, float rightDb)
{
	float magnitude[MAX_AUDIO_CHANNELS], peak[MAX_AUDIO_CHANNELS], input[MAX_AUDIO_CHANNELS];
	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i)
		magnitude[i] = peak[i] = input[i] = -std::numeric_limits<float>::infinity();
	peak[0] = leftDb;
	peak[1] = rightDb;
	std::lock_guard<std::mutex> lock(metersMutex);
	for (auto *meter : meters) {
		if (meter->source && meter->source->uuid == uuid && !meter->source->removed) {
			std::lock_guard<std::mutex> callbackLock(meter->callbackMutex);
			if (meter->callback)
				meter->callback(meter->context, magnitude, peak, input);
		}
	}
}
