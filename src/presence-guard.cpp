/*
Presence Guard for OBS Studio
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
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "presence-guard.hpp"
#include "platform-idle.hpp"

#include <plugin-support.h>

#include <chrono>
#include <cstdlib>

static const char *CFG_SECTION = "PresenceGuard";

/* How often the state machine runs. */
static const int POLL_INTERVAL_MS = 250;

/* If the volume meter has not produced a value for this long we assume the
 * microphone is not currently producing audio at all (not in an active scene,
 * muted at the fader, device unplugged) and suspend the dead-air rule instead
 * of declaring silence we cannot actually measure. */
static const int METER_STALE_MS = 3000;

static const int ATTACH_RETRY_MS = 3000;

static int64_t nowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

/* ------------------------------------------------------------------------- */
/* config helpers                                                            */
/* ------------------------------------------------------------------------- */

static bool cfgBool(config_t *config, const char *name, bool fallback)
{
	const char *value = config_get_string(config, CFG_SECTION, name);
	if (!value || !*value)
		return fallback;
	return strcmp(value, "true") == 0 || strcmp(value, "1") == 0;
}

static int cfgInt(config_t *config, const char *name, int fallback)
{
	const char *value = config_get_string(config, CFG_SECTION, name);
	if (!value || !*value)
		return fallback;
	return (int)strtol(value, nullptr, 10);
}

static std::string cfgStr(config_t *config, const char *name)
{
	const char *value = config_get_string(config, CFG_SECTION, name);
	return value ? std::string(value) : std::string();
}

/* ------------------------------------------------------------------------- */
/* volume meter callback (audio thread)                                      */
/* ------------------------------------------------------------------------- */

static void volmeter_updated(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
			     const float peak[MAX_AUDIO_CHANNELS], const float input_peak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(peak);
	UNUSED_PARAMETER(input_peak);

	auto *guard = static_cast<PresenceGuard *>(param);
	if (!guard)
		return;

	float loudest = -100.0f;
	for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (magnitude[i] > loudest)
			loudest = magnitude[i];
	}

	guard->onAudioLevel(loudest);
}

/* ------------------------------------------------------------------------- */
/* construction                                                              */
/* ------------------------------------------------------------------------- */

PresenceGuard::PresenceGuard(QObject *parent) : QObject(parent)
{
	quietSinceMs_ = nowMs();

	timer_ = new QTimer(this);
	timer_->setInterval(POLL_INTERVAL_MS);
	QObject::connect(timer_, &QTimer::timeout, this, [this]() { tick(); });
	timer_->start();
}

PresenceGuard::~PresenceGuard()
{
	if (timer_)
		timer_->stop();

	detachVolmeter();

	if (volmeter_) {
		obs_volmeter_remove_callback(volmeter_, volmeter_updated, this);
		obs_volmeter_destroy(volmeter_);
		volmeter_ = nullptr;
	}
}

/* ------------------------------------------------------------------------- */
/* settings                                                                  */
/* ------------------------------------------------------------------------- */

void PresenceGuard::applySettings(const PresenceSettings &next)
{
	const bool micChanged = next.micSource != settings.micSource;

	settings = next;

	/* Every settings change restarts the silence countdown, so a rule that
	 * has just been switched on never fires on stale bookkeeping. */
	quietSinceMs_ = nowMs();

	if (micChanged)
		refreshVolmeter();

	if (!settings.enabled)
		restoreNow();
}

void PresenceGuard::loadFromConfig(config_t *config)
{
	if (!config)
		return;

	PresenceSettings loaded;
	loaded.enabled = cfgBool(config, "Enabled", loaded.enabled);
	loaded.onlyWhileLive = cfgBool(config, "OnlyWhileLive", loaded.onlyWhileLive);
	loaded.deadAirEnabled = cfgBool(config, "DeadAirEnabled", loaded.deadAirEnabled);
	loaded.afkTimeoutSec = cfgInt(config, "AfkTimeoutSec", loaded.afkTimeoutSec);
	loaded.deadAirTimeoutSec = cfgInt(config, "DeadAirTimeoutSec", loaded.deadAirTimeoutSec);
	loaded.thresholdDb = cfgInt(config, "ThresholdDb", loaded.thresholdDb);
	loaded.afkScene = cfgStr(config, "AfkScene");
	loaded.deadAirScene = cfgStr(config, "DeadAirScene");
	loaded.micSource = cfgStr(config, "MicSource");

	applySettings(loaded);
}

void PresenceGuard::saveToConfig(config_t *config) const
{
	if (!config)
		return;

	config_set_bool(config, CFG_SECTION, "Enabled", settings.enabled);
	config_set_bool(config, CFG_SECTION, "OnlyWhileLive", settings.onlyWhileLive);
	config_set_bool(config, CFG_SECTION, "DeadAirEnabled", settings.deadAirEnabled);
	config_set_int(config, CFG_SECTION, "AfkTimeoutSec", settings.afkTimeoutSec);
	config_set_int(config, CFG_SECTION, "DeadAirTimeoutSec", settings.deadAirTimeoutSec);
	config_set_int(config, CFG_SECTION, "ThresholdDb", settings.thresholdDb);
	config_set_string(config, CFG_SECTION, "AfkScene", settings.afkScene.c_str());
	config_set_string(config, CFG_SECTION, "DeadAirScene", settings.deadAirScene.c_str());
	config_set_string(config, CFG_SECTION, "MicSource", settings.micSource.c_str());
}

void PresenceGuard::saveNow(void) const
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config)
		return;

	saveToConfig(config);
	config_save(config);
}

/* ------------------------------------------------------------------------- */
/* volume meter                                                              */
/* ------------------------------------------------------------------------- */

void PresenceGuard::ensureVolmeter(void)
{
	if (volmeter_)
		return;

	volmeter_ = obs_volmeter_create(OBS_FADER_LOG);
	if (!volmeter_) {
		obs_log(LOG_WARNING, "could not create the volume meter");
		return;
	}

	obs_volmeter_set_peak_meter_type(volmeter_, SAMPLE_PEAK_METER);
	obs_volmeter_add_callback(volmeter_, volmeter_updated, this);
}

void PresenceGuard::detachVolmeter(void)
{
	if (volmeter_)
		obs_volmeter_detach_source(volmeter_);

	if (meterSource_) {
		obs_source_release(meterSource_);
		meterSource_ = nullptr;
	}

	db_.store(-100.0f);
	lastCallbackMs_.store(0);
}

void PresenceGuard::refreshVolmeter(void)
{
	ensureVolmeter();

	if (meterSource_) {
		const char *current = obs_source_get_name(meterSource_);
		if (settings.micSource == (current ? current : ""))
			return; /* already attached to the right source */
	}

	detachVolmeter();

	if (!volmeter_ || settings.micSource.empty())
		return;

	obs_source_t *source = obs_get_source_by_name(settings.micSource.c_str());
	if (!source)
		return;

	if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO)) {
		obs_source_release(source);
		return;
	}

	if (!obs_volmeter_attach_source(volmeter_, source)) {
		obs_source_release(source);
		return;
	}

	/* Hold our own reference for as long as the meter is attached. */
	meterSource_ = source;
	quietSinceMs_ = nowMs();

	obs_log(LOG_INFO, "watching microphone '%s'", settings.micSource.c_str());
}

void PresenceGuard::onAudioLevel(float db)
{
	if (db != db) /* NaN, happens while the meter warms up */
		db = -100.0f;

	db_.store(db);
	lastCallbackMs_.store(nowMs());
}

/* ------------------------------------------------------------------------- */
/* scene helpers                                                             */
/* ------------------------------------------------------------------------- */

bool PresenceGuard::resolveScene(const std::string &name, obs_source_t **out) const
{
	*out = nullptr;

	if (name.empty())
		return false;

	obs_source_t *source = obs_get_source_by_name(name.c_str());
	if (!source)
		return false;

	if (!obs_scene_from_source(source)) {
		obs_source_release(source);
		return false;
	}

	*out = source;
	return true;
}

bool PresenceGuard::currentSceneIs(const std::string &name) const
{
	if (name.empty())
		return false;

	obs_source_t *current = obs_frontend_get_current_scene();
	if (!current)
		return false;

	const char *currentName = obs_source_get_name(current);
	const bool same = currentName && name == currentName;

	obs_source_release(current);
	return same;
}

bool PresenceGuard::liveOrRecording(void) const
{
	return obs_frontend_streaming_active() || obs_frontend_recording_active();
}

/* ------------------------------------------------------------------------- */
/* state machine                                                             */
/* ------------------------------------------------------------------------- */

void PresenceGuard::enterState(GuardState state, const std::string &sceneName)
{
	obs_source_t *scene = nullptr;
	if (!resolveScene(sceneName, &scene)) {
		/* Misconfiguration: back off for a few seconds instead of
		 * retrying (and logging) on every poll. */
		obs_log(LOG_WARNING, "scene '%s' does not exist, guard stays idle", sceneName.c_str());
		state_ = GuardState::Normal;
		quietSinceMs_ = nowMs();
		sceneBlockedUntilMs_ = nowMs() + 5000;
		return;
	}

	obs_source_t *current = obs_frontend_get_current_scene();
	savedScene_.clear();
	if (current) {
		const char *name = obs_source_get_name(current);
		if (name && sceneName != name)
			savedScene_ = name;
		obs_source_release(current);
	}

	if (!savedScene_.empty())
		obs_frontend_set_current_scene(scene);

	obs_source_release(scene);

	state_ = state;
	activeSceneName_ = sceneName;

	obs_log(LOG_INFO, "%s: showing scene '%s'", state == GuardState::Afk ? "AFK" : "dead air", sceneName.c_str());
}

void PresenceGuard::leaveState(void)
{
	state_ = GuardState::Normal;
	forcedAfk_ = false;
	quietSinceMs_ = nowMs();

	if (savedScene_.empty()) {
		activeSceneName_.clear();
		return;
	}

	/* Only hand the stream back if nothing else moved it in the meantime -
	 * if the operator switched scenes manually we leave their choice alone. */
	if (currentSceneIs(activeSceneName_)) {
		obs_source_t *scene = nullptr;
		if (resolveScene(savedScene_, &scene)) {
			obs_frontend_set_current_scene(scene);
			obs_source_release(scene);
			obs_log(LOG_INFO, "restored scene '%s'", savedScene_.c_str());
		}
	}

	savedScene_.clear();
	activeSceneName_.clear();
}

void PresenceGuard::restoreNow(void)
{
	if (state_ == GuardState::Normal) {
		forcedAfk_ = false;
		return;
	}

	leaveState();
}

void PresenceGuard::triggerAfkNow(void)
{
	if (settings.afkScene.empty()) {
		obs_log(LOG_WARNING, "no AFK scene configured");
		return;
	}

	restoreNow();
	forcedAfk_ = true;
	enterState(GuardState::Afk, settings.afkScene);
}

void PresenceGuard::tick(void)
{
	const int64_t now = nowMs();
	const bool live = liveOrRecording();
	const bool studio = obs_frontend_preview_program_mode_active();

	/* The microphone source may not exist yet when the module loads (scene
	 * collections are restored after modules), so keep retrying. */
	if (!meterSource_ && now - lastAttachAttemptMs_ >= ATTACH_RETRY_MS) {
		lastAttachAttemptMs_ = now;
		refreshVolmeter();
	}

	const float levelDb = db_.load();
	const int64_t callbackMs = lastCallbackMs_.load();
	const bool meterAlive = callbackMs > 0 && (now - callbackMs) < METER_STALE_MS;

	statusIdleSec_ = (int)(pg_idle_ms() / 1000ULL);
	statusDb_ = levelDb;
	statusHasMeter_ = meterSource_ != nullptr;

	const bool armed = settings.enabled && (!settings.onlyWhileLive || live);

	if (!armed) {
		if (state_ != GuardState::Normal)
			leaveState();
		quietSinceMs_ = now;
		return;
	}

	/* --- AFK rule ------------------------------------------------------ */

	const bool idleNow = statusIdleSec_ >= settings.afkTimeoutSec;
	const bool sceneBlocked = now < sceneBlockedUntilMs_;

	if (state_ == GuardState::Afk) {
		if (forcedAfk_)
			return;
		if (!idleNow)
			leaveState();
		quietSinceMs_ = now;
		return;
	}

	if (idleNow && !studio && !sceneBlocked) {
		if (state_ == GuardState::Normal)
			enterState(GuardState::Afk, settings.afkScene);
		quietSinceMs_ = now;
		return;
	}

	/* --- dead air rule ------------------------------------------------- */

	if (!settings.deadAirEnabled || !meterAlive || studio) {
		if (state_ == GuardState::DeadAir)
			leaveState();
		quietSinceMs_ = now;
		return;
	}

	const bool speaking = levelDb >= (float)settings.thresholdDb;
	if (speaking)
		quietSinceMs_ = now;

	if (state_ == GuardState::DeadAir) {
		if (speaking)
			leaveState();
		return;
	}

	if (!speaking && !sceneBlocked && (now - quietSinceMs_) >= (int64_t)settings.deadAirTimeoutSec * 1000) {
		const std::string &scene = settings.deadAirScene.empty() ? settings.afkScene : settings.deadAirScene;
		enterState(GuardState::DeadAir, scene);
	}
}

/* ------------------------------------------------------------------------- */
/* status                                                                    */
/* ------------------------------------------------------------------------- */

QString PresenceGuard::statusText(void) const
{
	if (!settings.enabled)
		return QString::fromUtf8(obs_module_text("PresenceGuard.Status.Off"));

	if (state_ == GuardState::Afk)
		return QString::fromUtf8(obs_module_text("PresenceGuard.Status.Afk"))
			.arg(QString::fromStdString(activeSceneName_));

	if (state_ == GuardState::DeadAir)
		return QString::fromUtf8(obs_module_text("PresenceGuard.Status.DeadAir"))
			.arg(QString::fromStdString(activeSceneName_));

	if (!statusHasMeter_)
		return QString::fromUtf8(obs_module_text("PresenceGuard.Status.NoMic"));

	if (!settings.deadAirEnabled)
		return QString::fromUtf8(obs_module_text("PresenceGuard.Status.AfkOnly")).arg(statusIdleSec_);

	return QString::fromUtf8(obs_module_text("PresenceGuard.Status.Watching"))
		.arg(statusIdleSec_)
		.arg(QString::number(statusDb_, 'f', 1))
		.arg(settings.thresholdDb);
}

const char *PresenceGuard::stateLabel(void) const
{
	switch (state_) {
	case GuardState::Afk:
		return obs_module_text("PresenceGuard.State.Afk");
	case GuardState::DeadAir:
		return obs_module_text("PresenceGuard.State.DeadAir");
	case GuardState::Normal:
	default:
		return obs_module_text("PresenceGuard.State.Normal");
	}
}
