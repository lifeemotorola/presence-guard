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

#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-audio-controls.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "director-core.hpp"

/*
 * Everything the operator can change, in one flat struct: saved to
 * settings.json next to the module, and rendered by the dock.
 */
struct DirectorSettings {
	bool enabled = false;
	/* Log what the director would do without actually cutting. */
	bool dryRun = false;
	bool onlyWhileLive = true;

	int thresholdDb = -45;
	int hysteresisDb = 3;
	int dwellMs = 4000;
	int fallbackDelaySec = 30;
	int operatorLockMs = 15000;

	std::string fallbackScene;
	std::vector<director::Take> takes;
};

/*
 * The Director owns the meters, the clock and the switching; the policy itself
 * lives in director::Core so it can be tested without OBS.
 *
 * Threading: the volume-meter callbacks run on the audio thread and only write
 * atomics. Every decision, every scene switch and every settings edit happens
 * on the Qt main thread, driven by a poll timer.
 */
class Director : public QObject {
public:
	explicit Director(QObject *parent = nullptr);
	~Director() override;

	/* Read by the dock on the Qt main thread. */
	DirectorSettings settings;

	void applySettings(const DirectorSettings &next);
	void loadSettings();
	void saveNow() const;

	/* (Re)reads the scene collection and attaches one meter per take. */
	void rebuildMeters();

	void handleEvent(enum obs_frontend_event event);

	/* Hotkey entry points. They queue the action for the poll timer. */
	void toggleEnabled();
	void nextTake();
	void lockNow();
	void fallbackNow();

	bool lockedNow() const;
	QString statusText() const;
	QString detailText() const;

	/* Live meter values for the dock, in dB. */
	float levelDb(size_t index) const;
	bool levelAlive(size_t index) const;

	/* The dock may outlive the director during module unload. */
	void detach() { detached_ = true; }

private:
	/* One camera: the scene, the microphone that belongs to it and the
	 * libobs volume meter measuring that microphone. */
	struct Slot {
		director::Take take;
		obs_source_t *source = nullptr;
		obs_volmeter_t *volmeter = nullptr;

		/* Written on the audio thread, read on the main thread. */
		std::atomic<float> db{-100.0f};
		std::atomic<int64_t> updatedMs{0};
		std::atomic<bool> haveSample{false};
	};

	static void onVolmeterUpdated(void *param, const float magnitude[], const float peak[],
				      const float inputPeak[]);

	void tick();
	void releaseMeters();
	void applyCoreConfig();
	void cutTo(const std::string &scene, const std::string &reason, bool manual);
	std::string currentSceneName() const;
	bool liveOrRecording() const;

	QTimer *timer_ = nullptr;
	director::Core core_;

	std::vector<std::unique_ptr<Slot>> slots_;

	/* Set while our own cut is in flight, so the scene-change event it
	 * causes is not mistaken for the operator taking over. */
	std::string expectedScene_;
	int64_t expectedUntilMs_ = 0;

	std::string lastReason_;
	std::string lastScene_;
	int64_t lastCutMs_ = 0;

	/* Written by the hotkey callbacks, which may run on any thread. */
	std::atomic<bool> togglePending_{false};
	std::atomic<bool> nextPending_{false};
	std::atomic<bool> lockPending_{false};
	std::atomic<bool> fallbackPending_{false};

	/* Nothing is metered or switched until OBS has finished loading. */
	bool frontendLoaded_ = false;
	bool detached_ = false;
};
