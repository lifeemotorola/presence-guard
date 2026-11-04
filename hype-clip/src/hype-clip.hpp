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

#include "hype-core.hpp"

/* One saved clip, as shown in the dock. */
struct ClipEntry {
	std::string path;
	int64_t savedMs = 0;
	float scoreDb = -100.0f;
	std::string reason;
};

/*
 * Watches every audio source in the scene collection and asks OBS's replay
 * buffer to save a clip when something clip-worthy happens.
 *
 * Threading: the volume-meter callbacks run on the audio thread and only write
 * atomics; every decision and every call into the frontend happens on the Qt
 * main thread, from the poll timer.
 */
class HypeClip : public QObject {
public:
	explicit HypeClip(QObject *parent = nullptr);
	~HypeClip() override;

	/* Read by the dock on the Qt main thread. */
	hype::Config config;

	void applyConfig(const hype::Config &next);
	void loadSettings();
	void saveNow() const;

	void handleEvent(enum obs_frontend_event event);

	/* Clip that. Used by the hotkey, the dock button and the detector. */
	void clipNow(const std::string &reason);
	void toggleAutoClip();

	hype::Status status() const;
	bool replayBufferActive() const;

	QString statusText() const;
	QString detailText() const;

	const std::vector<ClipEntry> &clips() const { return clips_; }
	void removeClip(size_t index);
	void clearClips();
	bool deleteClipFile(size_t index);
	std::string clipsFolder() const;

	/* The dock may outlive the plugin during module unload. */
	void detach() { detached_ = true; }

private:
	struct Slot {
		std::string name;
		obs_source_t *source = nullptr;
		obs_volmeter_t *volmeter = nullptr;

		std::atomic<float> db{-100.0f};
		std::atomic<int64_t> updatedMs{0};
		std::atomic<bool> haveSample{false};
	};

	static void onVolmeterUpdated(void *param, const float magnitude[], const float peak[],
				      const float inputPeak[]);

	void tick();
	void reconcileMeters();
	void releaseMeters();
	void requestClip(const std::string &reason, float scoreDb);
	void addClip(const std::string &path, float scoreDb, const std::string &reason);

	QTimer *timer_ = nullptr;
	hype::Detector detector_;

	std::vector<std::unique_ptr<Slot>> slots_;
	std::vector<ClipEntry> clips_;

	/* A clip we asked for, and when: the replay buffer's "saved" event
	 * arrives a moment later and has to be attributed to it. */
	bool clipPending_ = false;
	int64_t clipRequestedMs_ = 0;
	float clipScoreDb_ = -100.0f;
	std::string clipReason_;

	std::string lastMessage_;
	int64_t lastAutoClipMs_ = 0;
	int64_t lastReconcileMs_ = 0;

	bool frontendLoaded_ = false;
	bool detached_ = false;
};
