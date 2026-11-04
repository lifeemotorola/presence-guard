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

#include "auto-director.hpp"

#include <plugin-support.h>
#include <util/platform.h>

#include <chrono>
#include <cmath>
#include <cstring>

/* How often the director makes a decision. Fast enough to feel instant, slow
 * enough that the pace of the show is governed by the dwell time, not by the
 * timer. */
static const int POLL_INTERVAL_MS = 120;

/* A meter that has not produced a value for this long is treated as missing:
 * a muted fader, an unplugged device or a source that is not producing audio.
 * Missing meters are never read as silence, so a broken microphone can never
 * park the stream on the lobby scene. */
static const int METER_STALE_MS = 2500;

/* How long after our own cut a scene-change event still counts as ours. */
static const int SELF_CUT_GRACE_MS = 3000;

static const char *SETTINGS_FILE = "settings.json";

static int64_t nowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

static std::string safeStr(const char *value)
{
	return value ? std::string(value) : std::string();
}

/* True when the scene/microphone wiring changed. Flipping a take on or off
 * only changes the policy, so it must not tear the meters down. */
static bool wiringEqual(const std::vector<director::Take> &a, const std::vector<director::Take> &b)
{
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); i++) {
		if (a[i].scene != b[i].scene || a[i].source != b[i].source)
			return false;
	}

	return true;
}

/* Resolves a scene by name, for the current scene collection only. */
static obs_source_t *lookupScene(const std::string &name)
{
	if (name.empty())
		return nullptr;

	obs_source_t *source = obs_get_source_by_name(name.c_str());
	if (!source)
		return nullptr;

	if (obs_source_get_type(source) != OBS_SOURCE_TYPE_SCENE) {
		obs_source_release(source);
		return nullptr;
	}

	return source;
}

/* ------------------------------------------------------------------------- */
/* construction / persistence                                                */
/* ------------------------------------------------------------------------- */

Director::Director(QObject *parent) : QObject(parent)
{
	timer_ = new QTimer(this);
	timer_->setInterval(POLL_INTERVAL_MS);
	QObject::connect(timer_, &QTimer::timeout, [this]() { tick(); });
	timer_->start();

	applyCoreConfig();
}

Director::~Director()
{
	releaseMeters();
}

void Director::applyCoreConfig()
{
	director::Config config;
	config.enabled = settings.enabled;
	config.onlyWhileLive = settings.onlyWhileLive;
	config.thresholdDb = settings.thresholdDb;
	config.hysteresisDb = settings.hysteresisDb;
	config.dwellMs = settings.dwellMs;
	config.fallbackDelaySec = settings.fallbackDelaySec;
	config.fallbackScene = settings.fallbackScene;
	config.operatorLockMs = settings.operatorLockMs;
	core_.setConfig(config);
}

void Director::applySettings(const DirectorSettings &next)
{
	const bool rewired = !wiringEqual(settings.takes, next.takes);

	settings = next;
	applyCoreConfig();

	if (rewired)
		rebuildMeters();
	else
		core_.setTakes(settings.takes);

	saveNow();
}

void Director::loadSettings()
{
	char *path = obs_module_config_path(SETTINGS_FILE);
	if (!path)
		return;

	obs_data_t *data = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);

	if (!data) {
		obs_log(LOG_INFO, "no saved settings yet, starting with an empty take list");
		return;
	}

	settings.enabled = obs_data_get_bool(data, "enabled");
	settings.dryRun = obs_data_get_bool(data, "dryRun");
	settings.onlyWhileLive = obs_data_get_bool(data, "onlyWhileLive");

	settings.thresholdDb = (int)obs_data_get_int(data, "thresholdDb");
	settings.hysteresisDb = (int)obs_data_get_int(data, "hysteresisDb");
	settings.dwellMs = (int)obs_data_get_int(data, "dwellMs");
	settings.fallbackDelaySec = (int)obs_data_get_int(data, "fallbackDelaySec");
	settings.operatorLockMs = (int)obs_data_get_int(data, "operatorLockMs");

	settings.fallbackScene = safeStr(obs_data_get_string(data, "fallbackScene"));

	settings.takes.clear();
	obs_data_array_t *takes = obs_data_get_array(data, "takes");
	if (takes) {
		const size_t count = obs_data_array_count(takes);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(takes, i);
			if (!item)
				continue;

			director::Take take;
			take.scene = safeStr(obs_data_get_string(item, "scene"));
			take.source = safeStr(obs_data_get_string(item, "source"));
			take.enabled = obs_data_get_bool(item, "enabled");

			/* Missing booleans default to false; older files and
			 * hand-written ones should still load. */
			if (!obs_data_has_user_value(item, "enabled"))
				take.enabled = true;

			if (!take.scene.empty())
				settings.takes.push_back(take);

			obs_data_release(item);
		}
		obs_data_array_release(takes);
	}

	obs_data_release(data);

	obs_log(LOG_INFO, "loaded %zu take(s); director is %s", settings.takes.size(), settings.enabled ? "on" : "off");

	applyCoreConfig();
}

void Director::saveNow() const
{
	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "enabled", settings.enabled);
	obs_data_set_bool(data, "dryRun", settings.dryRun);
	obs_data_set_bool(data, "onlyWhileLive", settings.onlyWhileLive);
	obs_data_set_int(data, "thresholdDb", settings.thresholdDb);
	obs_data_set_int(data, "hysteresisDb", settings.hysteresisDb);
	obs_data_set_int(data, "dwellMs", settings.dwellMs);
	obs_data_set_int(data, "fallbackDelaySec", settings.fallbackDelaySec);
	obs_data_set_int(data, "operatorLockMs", settings.operatorLockMs);
	obs_data_set_string(data, "fallbackScene", settings.fallbackScene.c_str());

	obs_data_array_t *takes = obs_data_array_create();
	for (const director::Take &take : settings.takes) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "scene", take.scene.c_str());
		obs_data_set_string(item, "source", take.source.c_str());
		obs_data_set_bool(item, "enabled", take.enabled);
		obs_data_array_push_back(takes, item);
		obs_data_release(item);
	}
	obs_data_set_array(data, "takes", takes);
	obs_data_array_release(takes);

	char *path = obs_module_config_path(SETTINGS_FILE);
	if (path) {
		/* libobs does not create the per-module config folder for us. */
		char *dir = bstrdup(path);
		char *slash = strrchr(dir, '/');
		if (slash) {
			*slash = '\0';
			os_mkdirs(dir);
		}
		bfree(dir);

		if (!obs_data_save_json_safe(data, path, "tmp", "bak"))
			obs_log(LOG_WARNING, "could not save settings to '%s'", path);

		bfree(path);
	}

	obs_data_release(data);
}

/* ------------------------------------------------------------------------- */
/* meters                                                                    */
/* ------------------------------------------------------------------------- */

void Director::onVolmeterUpdated(void *param, const float magnitude[], const float peak[], const float inputPeak[])
{
	UNUSED_PARAMETER(peak);
	UNUSED_PARAMETER(inputPeak);

	Slot *slot = static_cast<Slot *>(param);
	if (!slot)
		return;

	float best = -100.0f;
	for (int channel = 0; channel < MAX_AUDIO_CHANNELS; channel++) {
		const float db = magnitude[channel];
		if (std::isnan(db) || std::isinf(db))
			continue;
		if (db > best)
			best = db;
	}

	if (best < -100.0f)
		best = -100.0f;

	slot->db.store(best, std::memory_order_relaxed);
	slot->updatedMs.store(nowMs(), std::memory_order_relaxed);
	slot->haveSample.store(true, std::memory_order_relaxed);
}

void Director::releaseMeters()
{
	for (std::unique_ptr<Slot> &slot : slots_) {
		if (slot->volmeter) {
			obs_volmeter_remove_callback(slot->volmeter, &Director::onVolmeterUpdated, slot.get());
			obs_volmeter_detach_source(slot->volmeter);
			obs_volmeter_destroy(slot->volmeter);
			slot->volmeter = nullptr;
		}

		if (slot->source) {
			obs_source_release(slot->source);
			slot->source = nullptr;
		}
	}

	slots_.clear();
}

void Director::rebuildMeters()
{
	releaseMeters();

	slots_.reserve(settings.takes.size());

	for (const director::Take &take : settings.takes) {
		std::unique_ptr<Slot> slot(new Slot());
		slot->take = take;

		obs_source_t *source = take.source.empty() ? nullptr : obs_get_source_by_name(take.source.c_str());
		if (take.source.empty()) {
			static bool warned = false;
			if (!warned) {
				warned = true;
				obs_log(LOG_WARNING,
					"a take has no microphone attached; it will never be cut to automatically");
			}
		} else if (!source) {
			obs_log(LOG_WARNING, "microphone '%s' does not exist (any more)", take.source.c_str());
		}

		if (source) {
			slot->source = source;

			obs_volmeter_t *volmeter = obs_volmeter_create(OBS_FADER_LOG);
			if (volmeter) {
				obs_volmeter_set_peak_meter_type(volmeter, SAMPLE_PEAK_METER);
				obs_volmeter_add_callback(volmeter, &Director::onVolmeterUpdated, slot.get());

				if (obs_volmeter_attach_source(volmeter, source)) {
					slot->volmeter = volmeter;
				} else {
					obs_volmeter_remove_callback(volmeter, &Director::onVolmeterUpdated,
								     slot.get());
					obs_volmeter_destroy(volmeter);
				}
			}
		}

		slots_.push_back(std::move(slot));
	}

	core_.setTakes(settings.takes);

	obs_log(LOG_INFO, "watching %zu take(s)", slots_.size());
}

float Director::levelDb(size_t index) const
{
	if (index >= slots_.size())
		return -100.0f;
	if (!levelAlive(index))
		return -100.0f;
	return slots_[index]->db.load(std::memory_order_relaxed);
}

bool Director::levelAlive(size_t index) const
{
	if (index >= slots_.size())
		return false;

	const Slot &slot = *slots_[index];
	if (!slot.volmeter || !slot.source || !slot.haveSample.load(std::memory_order_relaxed))
		return false;

	if (nowMs() - slot.updatedMs.load(std::memory_order_relaxed) > METER_STALE_MS)
		return false;

	return !obs_source_muted(slot.source);
}

/* ------------------------------------------------------------------------- */
/* the show                                                                  */
/* ------------------------------------------------------------------------- */

std::string Director::currentSceneName() const
{
	obs_source_t *scene = obs_frontend_get_current_scene();
	if (!scene)
		return std::string();

	std::string name = safeStr(obs_source_get_name(scene));
	obs_source_release(scene);
	return name;
}

bool Director::liveOrRecording() const
{
	return obs_frontend_streaming_active() || obs_frontend_recording_active();
}

bool Director::lockedNow() const
{
	return core_.locked(nowMs());
}

void Director::cutTo(const std::string &scene, const std::string &reason, bool manual)
{
	obs_source_t *target = lookupScene(scene);
	if (!target) {
		obs_log(LOG_WARNING, "cannot cut to '%s': no such scene in this collection", scene.c_str());
		lastReason_ = "scene '" + scene + "' is missing";
		return;
	}

	expectedScene_ = scene;
	expectedUntilMs_ = nowMs() + SELF_CUT_GRACE_MS;

	obs_frontend_set_current_scene(target);
	obs_source_release(target);

	lastScene_ = scene;
	lastReason_ = reason;
	lastCutMs_ = nowMs();

	if (manual)
		core_.noteOperatorChange(nowMs());

	obs_log(LOG_INFO, "cut to '%s': %s", scene.c_str(), reason.c_str());
}

void Director::tick()
{
	if (detached_ || !timer_ || !frontendLoaded_)
		return;

	const int64_t now = nowMs();

	/* Hotkey requests, applied on the main thread. */
	if (togglePending_.exchange(false)) {
		settings.enabled = !settings.enabled;
		applyCoreConfig();
		saveNow();
		obs_log(LOG_INFO, "director switched %s", settings.enabled ? "on" : "off");
	}

	if (lockPending_.exchange(false)) {
		core_.lock(now);
		lastReason_ = "held by the operator";
	}

	if (nextPending_.exchange(false)) {
		/* forceNext() is consumed by the next policy pass below. */
		core_.forceNext();
	}

	if (fallbackPending_.exchange(false)) {
		if (settings.fallbackScene.empty())
			lastReason_ = "no lobby scene is configured";
		else
			cutTo(settings.fallbackScene, "operator sent the lobby scene", true);
	}

	std::vector<director::Level> levels;
	levels.reserve(slots_.size());

	for (size_t i = 0; i < slots_.size(); i++) {
		director::Level level;
		level.alive = levelAlive(i);
		if (level.alive)
			level.db = slots_[i]->db.load(std::memory_order_relaxed);
		levels.push_back(level);
	}

	core_.submitLevels(now, levels);

	director::Decision decision =
		core_.tick(now, currentSceneName(), liveOrRecording(), obs_frontend_preview_program_mode_active());

	if (decision.action != director::Action::Switch)
		return;

	if (settings.dryRun) {
		/* Say it once per target instead of 8 times a second. */
		if (decision.scene != lastScene_ || now - lastCutMs_ > 5000) {
			lastScene_ = decision.scene;
			lastCutMs_ = now;
			lastReason_ = decision.reason + " (dry run, nothing was cut)";
			obs_log(LOG_INFO, "would cut to '%s': %s", decision.scene.c_str(), decision.reason.c_str());
		}
		return;
	}

	cutTo(decision.scene, decision.reason, false);

	/* An automatic cut to the lobby scene is not a manual intervention, but
	 * it does mean the show has gone quiet; nothing else to do here. */
}

void Director::handleEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		/* The scene collection is restored after modules load, so this is
		 * the first moment the configured scenes and microphones exist. */
		loadSettings();
		rebuildMeters();
		frontendLoaded_ = true;
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		/* Source objects are recreated with the collection. */
		rebuildMeters();
		break;

	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {
		const std::string current = currentSceneName();
		const int64_t now = nowMs();

		if (!expectedScene_.empty() && current == expectedScene_ && now <= expectedUntilMs_) {
			/* Our own cut coming back as an event. */
			expectedScene_.clear();
			break;
		}

		expectedScene_.clear();

		if (!settings.enabled || settings.dryRun)
			break;

		core_.noteOperatorChange(now);
		lastReason_ = "operator took over";
		break;
	}

	case OBS_FRONTEND_EVENT_EXIT:
		saveNow();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		lastReason_ = "on air";
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		lastReason_ = "off air";
		break;

	default:
		break;
	}
}

void Director::toggleEnabled()
{
	togglePending_.store(true);
}

void Director::nextTake()
{
	nextPending_.store(true);
}

void Director::lockNow()
{
	lockPending_.store(true);
}

void Director::fallbackNow()
{
	fallbackPending_.store(true);
}

/* ------------------------------------------------------------------------- */
/* status                                                                    */
/* ------------------------------------------------------------------------- */

QString Director::statusText() const
{
	if (detached_)
		return QString();

	if (!settings.enabled)
		return QString::fromUtf8(obs_module_text("AutoDirector.Status.Off"));

	if (settings.onlyWhileLive && !liveOrRecording())
		return QString::fromUtf8(obs_module_text("AutoDirector.Status.WaitingOffAir"));

	if (obs_frontend_preview_program_mode_active())
		return QString::fromUtf8(obs_module_text("AutoDirector.Status.StudioMode"));

	if (lockedNow())
		return QString::fromUtf8(obs_module_text("AutoDirector.Status.Locked"))
			.arg((int)(core_.lockedForMs(nowMs()) / 1000) + 1);

	if (settings.dryRun)
		return QString::fromUtf8(obs_module_text("AutoDirector.Status.DryRun"));

	if (!core_.anyMeterAlive())
		return QString::fromUtf8(obs_module_text("AutoDirector.Status.NoMeters"));

	return QString::fromUtf8(obs_module_text("AutoDirector.Status.Directing"));
}

QString Director::detailText() const
{
	if (detached_)
		return QString();

	const std::string scene = lastScene_.empty() ? currentSceneName() : lastScene_;

	if (lastReason_.empty())
		return QString::fromStdString(scene);

	return QString::fromStdString(scene + " - " + lastReason_);
}
