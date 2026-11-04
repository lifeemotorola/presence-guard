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

#include "hype-clip.hpp"

#include <plugin-support.h>
#include <util/platform.h>

#include <QFile>
#include <QFileInfo>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cmath>

/* How often the meters are read and the detector is asked. */
static const int POLL_INTERVAL_MS = 120;

/* How often the list of audio sources is re-read. Sources come and go with
 * scene collections and adds/removes; once a second is plenty. */
static const int RECONCILE_INTERVAL_MS = 1000;

/* A meter that has not reported for this long is treated as missing, so a
 * muted or unplugged microphone can never fake a loud moment. */
static const int METER_STALE_MS = 2500;

/* How long a "saved" event is attributed to the clip we asked for. */
static const int CLIP_ATTRIBUTION_MS = 15000;

static const size_t MAX_CLIPS = 40;

static const char *SETTINGS_FILE = "settings.json";

static int64_t nowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

static int64_t wallMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		       std::chrono::system_clock::now().time_since_epoch())
		.count();
}

static std::string safeStr(const char *value)
{
	return value ? std::string(value) : std::string();
}

/* Is this source something we can listen to?
 *
 * Scenes are skipped on purpose: a scene's audio is the sum of the sources
 * inside it, which are metered on their own, so listening to both would count
 * the same voice twice. */
static bool isAudible(obs_source_t *source)
{
	if (!source)
		return false;

	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE)
		return false;

	const uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_AUDIO) != 0;
}

HypeClip::HypeClip(QObject *parent) : QObject(parent)
{
	detector_.setConfig(config);

	timer_ = new QTimer(this);
	timer_->setInterval(POLL_INTERVAL_MS);
	QObject::connect(timer_, &QTimer::timeout, [this]() { tick(); });
	timer_->start();
}

HypeClip::~HypeClip()
{
	releaseMeters();
}

/* ------------------------------------------------------------------------- */
/* settings                                                                  */
/* ------------------------------------------------------------------------- */

void HypeClip::loadSettings()
{
	char *path = obs_module_config_path(SETTINGS_FILE);
	if (!path)
		return;

	obs_data_t *data = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);

	if (!data) {
		obs_log(LOG_INFO, "no saved settings yet, using the defaults");
		return;
	}

	config.enabled = obs_data_get_bool(data, "enabled");
	config.autoClip = obs_data_get_bool(data, "autoClip");
	config.thresholdDb = (int)obs_data_get_int(data, "thresholdDb");
	config.marginDb = (int)obs_data_get_int(data, "marginDb");
	config.sustainMs = (int)obs_data_get_int(data, "sustainMs");
	config.releaseMs = (int)obs_data_get_int(data, "releaseMs");
	config.cooldownMs = (int)obs_data_get_int(data, "cooldownMs");
	config.baselineMs = (int)obs_data_get_int(data, "baselineMs");

	/* The clip list survives a restart: streamers come back to it. */
	clips_.clear();
	obs_data_array_t *clips = obs_data_get_array(data, "clips");
	if (clips) {
		const size_t count = obs_data_array_count(clips);
		for (size_t i = 0; i < count && clips_.size() < MAX_CLIPS; i++) {
			obs_data_t *item = obs_data_array_item(clips, i);
			if (!item)
				continue;

			ClipEntry entry;
			entry.path = safeStr(obs_data_get_string(item, "path"));
			entry.savedMs = (int64_t)obs_data_get_int(item, "savedMs");
			entry.scoreDb = (float)obs_data_get_double(item, "scoreDb");
			entry.reason = safeStr(obs_data_get_string(item, "reason"));

			if (!entry.path.empty())
				clips_.push_back(entry);

			obs_data_release(item);
		}
		obs_data_array_release(clips);
	}

	obs_data_release(data);

	detector_.setConfig(config);

	obs_log(LOG_INFO, "loaded %zu clip(s); auto clipping is %s", clips_.size(),
		config.enabled && config.autoClip ? "on" : "off");
}

void HypeClip::saveNow() const
{
	obs_data_t *data = obs_data_create();

	obs_data_set_bool(data, "enabled", config.enabled);
	obs_data_set_bool(data, "autoClip", config.autoClip);
	obs_data_set_int(data, "thresholdDb", config.thresholdDb);
	obs_data_set_int(data, "marginDb", config.marginDb);
	obs_data_set_int(data, "sustainMs", config.sustainMs);
	obs_data_set_int(data, "releaseMs", config.releaseMs);
	obs_data_set_int(data, "cooldownMs", config.cooldownMs);
	obs_data_set_int(data, "baselineMs", config.baselineMs);

	obs_data_array_t *clips = obs_data_array_create();
	for (const ClipEntry &entry : clips_) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "path", entry.path.c_str());
		obs_data_set_int(item, "savedMs", (long long)entry.savedMs);
		obs_data_set_double(item, "scoreDb", (double)entry.scoreDb);
		obs_data_set_string(item, "reason", entry.reason.c_str());
		obs_data_array_push_back(clips, item);
		obs_data_release(item);
	}
	obs_data_set_array(data, "clips", clips);
	obs_data_array_release(clips);

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

void HypeClip::applyConfig(const hype::Config &next)
{
	config = next;
	detector_.setConfig(config);
	saveNow();
}

/* ------------------------------------------------------------------------- */
/* meters                                                                    */
/* ------------------------------------------------------------------------- */

void HypeClip::onVolmeterUpdated(void *param, const float magnitude[], const float peak[], const float inputPeak[])
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

void HypeClip::releaseMeters()
{
	for (std::unique_ptr<Slot> &slot : slots_) {
		if (slot->volmeter) {
			obs_volmeter_remove_callback(slot->volmeter, &HypeClip::onVolmeterUpdated, slot.get());
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

namespace {

struct SourceCollector {
	std::vector<std::string> names;
};

bool collectAudible(void *param, obs_source_t *source)
{
	if (!isAudible(source))
		return true;

	const char *name = obs_source_get_name(source);
	/* Filters and other private sources have no user-facing name. */
	if (!name || !*name)
		return true;

	SourceCollector *collector = static_cast<SourceCollector *>(param);
	collector->names.push_back(name);
	return true;
}

} // namespace

void HypeClip::reconcileMeters()
{
	SourceCollector collector;
	obs_enum_sources(collectAudible, &collector);

	/* Same set of names, same meters: nothing to do. */
	if (collector.names.size() == slots_.size()) {
		bool same = true;
		for (size_t i = 0; i < slots_.size() && same; i++)
			same = slots_[i]->name == collector.names[i];

		if (same)
			return;
	}

	releaseMeters();

	slots_.reserve(collector.names.size());

	for (const std::string &name : collector.names) {
		obs_source_t *source = obs_get_source_by_name(name.c_str());
		if (!source)
			continue;

		std::unique_ptr<Slot> slot(new Slot());
		slot->name = name;
		slot->source = source;

		obs_volmeter_t *volmeter = obs_volmeter_create(OBS_FADER_LOG);
		if (volmeter) {
			obs_volmeter_set_peak_meter_type(volmeter, SAMPLE_PEAK_METER);
			obs_volmeter_add_callback(volmeter, &HypeClip::onVolmeterUpdated, slot.get());

			if (obs_volmeter_attach_source(volmeter, source)) {
				slot->volmeter = volmeter;
			} else {
				obs_volmeter_remove_callback(volmeter, &HypeClip::onVolmeterUpdated, slot.get());
				obs_volmeter_destroy(volmeter);
			}
		}

		slots_.push_back(std::move(slot));
	}

	obs_log(LOG_INFO, "listening to %zu audio source(s)", slots_.size());
}

/* ------------------------------------------------------------------------- */
/* clipping                                                                  */
/* ------------------------------------------------------------------------- */

bool HypeClip::replayBufferActive() const
{
	return obs_frontend_replay_buffer_active();
}

void HypeClip::requestClip(const std::string &reason, float scoreDb)
{
	if (!obs_frontend_replay_buffer_active()) {
		lastMessage_ = "the replay buffer is not running";

		if (config.enabled) {
			/* Start it, so the next moment can be clipped. A buffer
			 * that has just started is still empty, so this moment
			 * is lost - say so instead of pretending. */
			obs_frontend_replay_buffer_start();
			lastMessage_ = "the replay buffer was off, so it was started for the next moment";
			obs_log(LOG_WARNING, "%s", lastMessage_.c_str());
		}

		return;
	}

	clipPending_ = true;
	clipRequestedMs_ = nowMs();
	clipScoreDb_ = scoreDb;
	clipReason_ = reason;

	obs_frontend_replay_buffer_save();

	obs_log(LOG_INFO, "clip requested (%s, %.0f dB)", reason.c_str(), scoreDb);
}

void HypeClip::clipNow(const std::string &reason)
{
	const hype::Status current = detector_.status(nowMs());
	requestClip(reason, current.db);

	/* A manual clip starts the cooldown too, so a shout does not follow it
	 * with an automatic duplicate. */
	detector_.noteClip(nowMs(), current.db);
}

void HypeClip::toggleAutoClip()
{
	config.autoClip = !config.autoClip;
	applyConfig(config);
}

void HypeClip::addClip(const std::string &path, float scoreDb, const std::string &reason)
{
	if (path.empty())
		return;

	ClipEntry entry;
	entry.path = path;
	entry.savedMs = wallMs();
	entry.scoreDb = scoreDb;
	entry.reason = reason;

	clips_.insert(clips_.begin(), entry);
	if (clips_.size() > MAX_CLIPS)
		clips_.resize(MAX_CLIPS);

	lastMessage_ = "clip saved";
	saveNow();
}

void HypeClip::removeClip(size_t index)
{
	if (index >= clips_.size())
		return;

	clips_.erase(clips_.begin() + (long)index);
	saveNow();
}

void HypeClip::clearClips()
{
	clips_.clear();
	saveNow();
}

bool HypeClip::deleteClipFile(size_t index)
{
	if (index >= clips_.size())
		return false;

	const QString path = QString::fromStdString(clips_[index].path);
	const bool removed = QFile::remove(path);

	if (!removed) {
		lastMessage_ = "the file could not be deleted";
		return false;
	}

	removeClip(index);
	lastMessage_ = "clip deleted";
	return true;
}

std::string HypeClip::clipsFolder() const
{
	if (!clips_.empty())
		return QFileInfo(QString::fromStdString(clips_.front().path)).absolutePath().toStdString();

	char *dir = obs_frontend_get_current_record_output_path();
	if (!dir)
		return std::string();

	std::string folder = dir;
	bfree(dir);
	return folder;
}

/* ------------------------------------------------------------------------- */
/* the loop                                                                  */
/* ------------------------------------------------------------------------- */

void HypeClip::tick()
{
	if (detached_ || !timer_ || !frontendLoaded_)
		return;

	const int64_t now = nowMs();

	if (now - lastReconcileMs_ >= RECONCILE_INTERVAL_MS) {
		lastReconcileMs_ = now;
		reconcileMeters();
	}

	std::vector<hype::Level> levels;
	levels.reserve(slots_.size());

	for (const std::unique_ptr<Slot> &slot : slots_) {
		hype::Level level;

		const bool alive = slot->volmeter && slot->source && slot->haveSample.load(std::memory_order_relaxed) &&
				   (now - slot->updatedMs.load(std::memory_order_relaxed) < METER_STALE_MS) &&
				   !obs_source_muted(slot->source);

		level.alive = alive;
		if (alive)
			level.db = slot->db.load(std::memory_order_relaxed);

		levels.push_back(level);
	}

	detector_.submit(now, levels);

	const hype::Request request = detector_.tick(now);
	if (!request.wanted)
		return;

	/* Do not request clips more often than the detector's cooldown, even if
	 * the replay buffer was missing for the first request. */
	const int64_t cooldownMs = (int64_t)config.cooldownMs;
	if (lastAutoClipMs_ > 0 && now - lastAutoClipMs_ < cooldownMs)
		return;

	lastAutoClipMs_ = now;
	requestClip("hype", request.scoreDb);
}

void HypeClip::handleEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		loadSettings();
		detector_.setConfig(config);
		frontendLoaded_ = true;
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		releaseMeters();
		break;

	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED: {
		char *path = obs_frontend_get_last_replay();
		if (!path)
			break;

		const std::string file = path;
		bfree(path);

		const int64_t now = nowMs();
		const bool ours = clipPending_ && (now - clipRequestedMs_) <= CLIP_ATTRIBUTION_MS;

		addClip(file, ours ? clipScoreDb_ : -100.0f, ours ? clipReason_ : "saved in OBS");
		clipPending_ = false;

		obs_log(LOG_INFO, "clip saved: %s", file.c_str());
		break;
	}

	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		lastMessage_ = "the replay buffer stopped";
		break;

	case OBS_FRONTEND_EVENT_EXIT:
		saveNow();
		break;

	default:
		break;
	}
}

/* ------------------------------------------------------------------------- */
/* status                                                                    */
/* ------------------------------------------------------------------------- */

hype::Status HypeClip::status() const
{
	return detector_.status(nowMs());
}

QString HypeClip::statusText() const
{
	if (detached_)
		return QString();

	if (!config.enabled)
		return QString::fromUtf8(obs_module_text("HypeClip.Status.Off"));

	if (!config.autoClip)
		return QString::fromUtf8(obs_module_text("HypeClip.Status.ManualOnly"));

	if (!replayBufferActive())
		return QString::fromUtf8(obs_module_text("HypeClip.Status.NoBuffer"));

	const hype::Status current = status();

	if (current.cooldownLeftMs > 0)
		return QString::fromUtf8(obs_module_text("HypeClip.Status.Cooldown"))
			.arg((int)(current.cooldownLeftMs / 1000) + 1);

	return QString::fromUtf8(obs_module_text("HypeClip.Status.Watching"));
}

QString HypeClip::detailText() const
{
	if (detached_)
		return QString();

	const hype::Status current = status();

	QString text = QString::fromUtf8(obs_module_text("HypeClip.Detail.Meter"))
			       .arg((int)current.db)
			       .arg((int)current.baselineDb);

	if (!lastMessage_.empty())
		text += QString::fromUtf8(" - ") + QString::fromStdString(lastMessage_);

	return text;
}
