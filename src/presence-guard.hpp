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

#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-audio-controls.h>
#include <util/config-file.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <string>

enum class GuardState {
	Normal = 0,
	Afk,
	DeadAir,
};

struct PresenceSettings {
	bool enabled = false;
	bool onlyWhileLive = true;
	bool deadAirEnabled = true;

	int afkTimeoutSec = 120;
	int deadAirTimeoutSec = 45;
	int thresholdDb = -45;

	std::string afkScene;
	std::string deadAirScene;
	std::string micSource;
};

/*
 * PresenceGuard owns all of the decision making:
 *
 *  - the AFK rule compares the system-wide idle time against a threshold and
 *    parks the stream on a safe scene while the operator is away,
 *  - the dead-air rule watches the chosen microphone with a libobs volume
 *    meter and does the same when nobody has spoken for a while,
 *  - both rules remember which scene was live and hand it back on return.
 *
 * All scene switching happens on the Qt main thread via the poll timer; the
 * audio callback only ever writes atomics.
 */
class PresenceGuard : public QObject {
public:
	explicit PresenceGuard(QObject *parent = nullptr);
	~PresenceGuard() override;

	/* settings is read/written from the Qt main thread only. */
	PresenceSettings settings;

	void applySettings(const PresenceSettings &next);

	/* (Re)attach the volume meter to the configured microphone source. */
	void refreshVolmeter(void);

	/* Drops any temporary scene the guard put up. Safe to call any time. */
	void restoreNow(void);

	/* Fire the AFK rule immediately, for testing. */
	void triggerAfkNow(void);

	void loadFromConfig(config_t *config);
	void saveToConfig(config_t *config) const;
	void saveNow(void) const;

	/* Audio-thread entry point (writes atomics only). */
	void onAudioLevel(float db);

	GuardState state(void) const { return state_; }
	QString statusText(void) const;
	const char *stateLabel(void) const;

private:
	void tick(void);

	void ensureVolmeter(void);
	void detachVolmeter(void);

	void enterState(GuardState state, const std::string &sceneName);
	void leaveState(void);

	bool resolveScene(const std::string &name, obs_source_t **out) const;
	bool currentSceneIs(const std::string &name) const;
	bool liveOrRecording(void) const;

	QTimer *timer_ = nullptr;

	GuardState state_ = GuardState::Normal;
	bool forcedAfk_ = false;
	std::string savedScene_;
	std::string activeSceneName_;

	obs_volmeter_t *volmeter_ = nullptr;
	obs_source_t *meterSource_ = nullptr;

	/* written by the audio thread, read by the poll timer */
	std::atomic<float> db_{-100.0f};
	std::atomic<int64_t> lastCallbackMs_{0};

	int64_t quietSinceMs_ = 0;
	int64_t lastAttachAttemptMs_ = 0;
	int64_t sceneBlockedUntilMs_ = 0;

	/* cached for statusText() */
	int statusIdleSec_ = 0;
	float statusDb_ = -100.0f;
	bool statusHasMeter_ = false;
};
