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

/*
 * Standalone harness for PresenceGuard's state machine.
 *
 * It links the real src/presence-guard.cpp against a tiny fake libobs
 * (tests/stubs + the definitions below) so the rules can be exercised without
 * a running OBS. Build and run with tests/run-tests.sh.
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <plugin-support.h>

#include "presence-guard.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

/* ------------------------------------------------------------------------- */
/* fake libobs                                                               */
/* ------------------------------------------------------------------------- */

struct obs_source {
	std::string name;
	bool isScene = false;
	bool isAudio = false;
};

struct obs_volmeter {
	obs_volmeter_updated_t callback = nullptr;
	void *param = nullptr;
	obs_source_t *source = nullptr;
};

struct config_data {
	std::map<std::string, std::string> values;
};

static std::vector<obs_source_t *> g_sources;
static obs_source_t *g_currentScene = nullptr;
static bool g_streaming = false;
static bool g_recording = false;
static bool g_studioMode = false;
static obs_volmeter_t *g_volmeter = nullptr;
static config_data g_config;
static int g_volmeterCreated = 0;

extern uint64_t g_testIdleMs;

obs_source_t *obs_get_source_by_name(const char *name)
{
	for (obs_source_t *source : g_sources) {
		if (source->name == name)
			return source;
	}
	return nullptr;
}

void obs_source_release(obs_source_t *) {}
const char *obs_source_get_name(const obs_source_t *source)
{
	return source ? source->name.c_str() : nullptr;
}

uint32_t obs_source_get_output_flags(const obs_source_t *source)
{
	return (source && source->isAudio) ? OBS_SOURCE_AUDIO : 0;
}

obs_scene_t *obs_scene_from_source(const obs_source_t *source)
{
	return (source && source->isScene) ? (obs_scene_t *)source : nullptr;
}

void obs_enum_sources(bool (*enum_proc)(void *, obs_source_t *), void *param)
{
	for (obs_source_t *source : g_sources) {
		if (!enum_proc(param, source))
			break;
	}
}

obs_volmeter_t *obs_volmeter_create(enum obs_fader_type)
{
	if (!g_volmeter) {
		g_volmeter = new obs_volmeter();
		g_volmeterCreated++;
	}
	return g_volmeter;
}

void obs_volmeter_destroy(obs_volmeter_t *volmeter)
{
	if (g_volmeter == volmeter)
		g_volmeter = nullptr;
	delete volmeter;
}

bool obs_volmeter_attach_source(obs_volmeter_t *volmeter, obs_source_t *source)
{
	volmeter->source = source;
	return source != nullptr;
}

void obs_volmeter_detach_source(obs_volmeter_t *volmeter)
{
	volmeter->source = nullptr;
}

void obs_volmeter_set_peak_meter_type(obs_volmeter_t *, enum obs_peak_meter_type) {}

void obs_volmeter_add_callback(obs_volmeter_t *volmeter, obs_volmeter_updated_t callback, void *param)
{
	volmeter->callback = callback;
	volmeter->param = param;
}

void obs_volmeter_remove_callback(obs_volmeter_t *volmeter, obs_volmeter_updated_t, void *)
{
	volmeter->callback = nullptr;
	volmeter->param = nullptr;
}

void obs_frontend_get_scenes(struct obs_frontend_source_list *sources)
{
	for (obs_source_t *source : g_sources) {
		if (source->isScene) {
			sources->sources.array = nullptr;
			sources->sources.num = 0;
			break;
		}
	}
}

obs_source_t *obs_frontend_get_current_scene(void)
{
	return g_currentScene;
}

void obs_frontend_set_current_scene(obs_source_t *scene)
{
	g_currentScene = scene;
}

bool obs_frontend_streaming_active(void)
{
	return g_streaming;
}

bool obs_frontend_recording_active(void)
{
	return g_recording;
}

bool obs_frontend_preview_program_mode_active(void)
{
	return g_studioMode;
}

config_t *obs_frontend_get_profile_config(void)
{
	return &g_config;
}

int config_save(config_t *)
{
	return 0;
}

static std::string key(const char *section, const char *name)
{
	return std::string(section) + "/" + name;
}

void config_set_string(config_t *config, const char *section, const char *name, const char *value)
{
	config->values[key(section, name)] = value ? value : "";
}

void config_set_int(config_t *config, const char *section, const char *name, int64_t value)
{
	config->values[key(section, name)] = std::to_string(value);
}

void config_set_bool(config_t *config, const char *section, const char *name, bool value)
{
	config->values[key(section, name)] = value ? "true" : "false";
}

const char *config_get_string(config_t *config, const char *section, const char *name)
{
	auto it = config->values.find(key(section, name));
	return it == config->values.end() ? nullptr : it->second.c_str();
}

int64_t config_get_int(config_t *config, const char *section, const char *name)
{
	const char *value = config_get_string(config, section, name);
	return value ? strtoll(value, nullptr, 10) : 0;
}

bool config_get_bool(config_t *config, const char *section, const char *name)
{
	const char *value = config_get_string(config, section, name);
	return value && strcmp(value, "true") == 0;
}

/* ------------------------------------------------------------------------- */
/* helpers                                                                   */
/* ------------------------------------------------------------------------- */

static int g_failures = 0;

static void check(bool condition, const char *what)
{
	std::printf("  %s  %s\n", condition ? "PASS" : "FAIL", what);
	if (!condition)
		g_failures++;
}

static void pump(int milliseconds)
{
	QElapsedTimer timer;
	timer.start();

	while (timer.elapsed() < milliseconds) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		QThread::msleep(5);
	}
}

/* Feeds a level into the real audio-callback path for the given duration. */
static void speak(int milliseconds, float db)
{
	float magnitude[MAX_AUDIO_CHANNELS];
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
		magnitude[i] = db;

	float peak[MAX_AUDIO_CHANNELS] = {0};

	QElapsedTimer timer;
	timer.start();

	while (timer.elapsed() < milliseconds) {
		if (g_volmeter && g_volmeter->callback)
			g_volmeter->callback(g_volmeter->param, magnitude, peak, peak);

		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		QThread::msleep(20);
	}
}

static bool showing(const char *sceneName)
{
	return g_currentScene && g_currentScene->name == sceneName;
}

/* ------------------------------------------------------------------------- */
/* tests                                                                     */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);

	obs_source_t *mainScene = new obs_source{"Main", true, false};
	obs_source_t *brbScene = new obs_source{"BRB", true, false};
	obs_source_t *silenceScene = new obs_source{"Silence", true, false};
	obs_source_t *mic = new obs_source{"Mic", false, true};
	obs_source_t *webcam = new obs_source{"Webcam", false, false};

	g_sources = {mainScene, brbScene, silenceScene, mic, webcam};

	g_streaming = true;

	/* -- 1. AFK rule parks the stream and hands the scene back ---------- */
	std::printf("AFK rule\n");
	{
		PresenceGuard guard;
		PresenceSettings settings;
		settings.enabled = true;
		settings.afkTimeoutSec = 1;
		settings.afkScene = "BRB";
		settings.deadAirEnabled = false;
		guard.applySettings(settings);

		g_currentScene = mainScene;
		g_testIdleMs = 4000;
		pump(600);
		check(guard.state() == GuardState::Afk, "guard reports AFK");
		check(showing("BRB"), "AFK scene is live");

		g_testIdleMs = 0;
		pump(400);
		check(guard.state() == GuardState::Normal, "guard reports normal");
		check(showing("Main"), "previous scene restored");
	}

	/* -- 2. Idle is ignored when the stream is not running -------------- */
	std::printf("only-while-live\n");
	{
		PresenceGuard guard;
		PresenceSettings settings;
		settings.enabled = true;
		settings.onlyWhileLive = true;
		settings.afkTimeoutSec = 1;
		settings.afkScene = "BRB";
		settings.deadAirEnabled = false;
		guard.applySettings(settings);

		g_currentScene = mainScene;
		g_streaming = false;
		g_testIdleMs = 4000;
		pump(600);
		check(showing("Main"), "no switch while idle but offline");

		g_streaming = true;
		pump(600);
		check(showing("BRB"), "switches once streaming starts");

		g_testIdleMs = 0;
		pump(400);
		g_streaming = true;
	}

	/* -- 3. Dead-air rule ---------------------------------------------- */
	std::printf("dead-air rule\n");
	{
		PresenceGuard guard;
		PresenceSettings settings;
		settings.enabled = true;
		settings.afkTimeoutSec = 600;
		settings.afkScene = "BRB";
		settings.deadAirEnabled = true;
		settings.deadAirScene = "Silence";
		settings.deadAirTimeoutSec = 1;
		settings.thresholdDb = -40;
		settings.micSource = "Mic";
		guard.applySettings(settings);

		check(g_volmeter != nullptr && g_volmeter->source == mic, "volume meter attached to the mic");

		g_currentScene = mainScene;
		speak(1400, -70.0f);
		check(guard.state() == GuardState::DeadAir, "guard reports dead air");
		check(showing("Silence"), "dead-air scene is live");

		speak(700, -20.0f);
		check(guard.state() == GuardState::Normal, "guard reports normal");
		check(showing("Main"), "previous scene restored");
	}

	/* -- 4. A meter with no data must not be mistaken for silence ------- */
	std::printf("stale meter\n");
	{
		PresenceGuard guard;
		PresenceSettings settings;
		settings.enabled = true;
		settings.afkTimeoutSec = 600;
		settings.afkScene = "BRB";
		settings.deadAirEnabled = true;
		settings.deadAirScene = "Silence";
		settings.deadAirTimeoutSec = 1;
		settings.thresholdDb = -40;
		settings.micSource = "";
		guard.applySettings(settings);

		check(g_volmeter == nullptr || g_volmeter->source == nullptr, "no meter attached");

		g_currentScene = mainScene;
		pump(1400);
		check(showing("Main"), "silence without a meter changes nothing");
	}

	/* -- 5. Manual scene changes win ------------------------------------ */
	std::printf("operator override\n");
	{
		PresenceGuard guard;
		PresenceSettings settings;
		settings.enabled = true;
		settings.afkTimeoutSec = 1;
		settings.afkScene = "BRB";
		settings.deadAirEnabled = false;
		guard.applySettings(settings);

		g_currentScene = mainScene;
		g_testIdleMs = 4000;
		pump(600);
		check(showing("BRB"), "AFK scene is live");

		g_currentScene = silenceScene; /* operator clicks a scene */
		g_testIdleMs = 0;
		pump(400);
		check(showing("Silence"), "guard does not yank the operator back");
	}

	/* -- 6. A missing scene must not wedge the guard -------------------- */
	std::printf("missing scene\n");
	{
		PresenceGuard guard;
		PresenceSettings settings;
		settings.enabled = true;
		settings.afkTimeoutSec = 1;
		settings.afkScene = "Does Not Exist";
		settings.deadAirEnabled = false;
		guard.applySettings(settings);

		g_currentScene = mainScene;
		g_testIdleMs = 4000;
		pump(700);
		check(showing("Main"), "nothing happens without a valid scene");
		check(guard.state() == GuardState::Normal, "guard stays normal");

		g_testIdleMs = 0;
		pump(400);
	}

	/* -- 7. Settings survive a save/load round trip --------------------- */
	std::printf("settings round trip\n");
	{
		PresenceGuard guard;
		PresenceSettings settings;
		settings.enabled = true;
		settings.onlyWhileLive = false;
		settings.afkTimeoutSec = 90;
		settings.afkScene = "BRB";
		settings.deadAirEnabled = true;
		settings.deadAirScene = "Silence";
		settings.deadAirTimeoutSec = 30;
		settings.thresholdDb = -33;
		settings.micSource = "Mic";
		guard.applySettings(settings);
		guard.saveNow();

		PresenceGuard reloaded;
		reloaded.loadFromConfig(&g_config);

		check(reloaded.settings.enabled, "enabled persisted");
		check(!reloaded.settings.onlyWhileLive, "only-while-live persisted");
		check(reloaded.settings.afkTimeoutSec == 90, "AFK timeout persisted");
		check(reloaded.settings.afkScene == "BRB", "AFK scene persisted");
		check(reloaded.settings.deadAirEnabled, "dead-air flag persisted");
		check(reloaded.settings.deadAirScene == "Silence", "dead-air scene persisted");
		check(reloaded.settings.deadAirTimeoutSec == 30, "dead-air timeout persisted");
		check(reloaded.settings.thresholdDb == -33, "threshold persisted");
		check(reloaded.settings.micSource == "Mic", "microphone persisted");
	}

	std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_failures,
		    g_failures == 1 ? "" : "s");

	return g_failures == 0 ? 0 : 1;
}
