/*
Auto Scene Adjust for OBS Studio
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

#include <QObject>
#include <QString>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "scene-core.hpp"

/* One source in the current scene, as the dock sees it. */
struct SceneRow {
	int64_t id = 0;
	std::string name;
	int srcW = 0;
	int srcH = 0;
	autoscene::Rect rect;
	autoscene::Issue issue = autoscene::Issue::Fine;
	autoscene::FitMode mode = autoscene::FitMode::None;
	bool locked = false;
	bool group = false;
	bool visible = true;

	bool canAdjust() const { return !locked && !group && srcW > 0 && srcH > 0; }
};

struct AdjustConfig {
	bool autoRefit = true;
	int margin = 0;
	int gap = 8;
	autoscene::Align align = autoscene::Align::Center;
};

/*
 * Keeps the current scene honest: it measures every source, says which ones
 * are off canvas, cut off, tiny or squashed, and fixes the ones the streamer
 * asked it to look after whenever the canvas or the source itself changes
 * size.
 *
 * Threading: everything happens on the Qt main thread, from the poll timer or
 * from a dock button. No callbacks from other threads.
 */
class SceneAdjust : public QObject {
public:
	explicit SceneAdjust(QObject *parent = nullptr);
	~SceneAdjust() override;

	AdjustConfig config;

	void loadSettings();
	void saveNow() const;

	void handleEvent(enum obs_frontend_event event);

	/* Re-reads the scene. Called by the poll timer and the dock. */
	void refresh();

	const std::vector<SceneRow> &rows() const { return rows_; }
	std::string sceneName() const { return sceneName_; }
	int canvasWidth() const { return canvasW_; }
	int canvasHeight() const { return canvasH_; }
	bool canvasKnown() const { return canvasW_ > 0 && canvasH_ > 0; }

	int problemCount() const;
	int adjustableCount() const;

	/* Adopt (or release) a source. Adopting applies the mode immediately. */
	void setMode(int64_t id, autoscene::FitMode mode);
	void fixRow(int64_t id);

	/* Fix everything in the scene that can be fixed. */
	void fixScene();
	void applyLayout(autoscene::Layout layout);

	bool canUndo() const { return !undo_.empty(); }
	void undo();

	void setAutoRefit(bool on);
	void setMargin(int margin);
	void setGap(int gap);
	void setAlign(autoscene::Align align);

	QString sceneText() const;
	QString summaryText() const;
	QString lastAction() const { return lastAction_; }

	/* The dock may outlive the plugin during module unload. */
	void detach() { detached_ = true; }

private:
	struct SnapshotEntry {
		std::string scene;
		int64_t id = 0;
		obs_transform_info info;
	};

	static std::string keyFor(const std::string &scene, int64_t id);
	static autoscene::Item measure(const obs_transform_info &info, int srcW, int srcH);
	static bool placeItem(obs_sceneitem_t *item, const autoscene::Placement &placement);

	void tick();
	void refreshRows();
	void maybeAutoRefit();
	void refitEverything();
	void rememberWatches();

	void snapshotScene(const std::string &scene);
	void snapshotAll();
	bool applyPlacement(int64_t id, autoscene::FitMode mode, const autoscene::Rect *cell);
	bool applyMode(int64_t id, autoscene::FitMode mode, bool report = true);
	void setMessage(const char *key, int count = -1);

	autoscene::FitMode managedMode(const std::string &scene, int64_t id) const;

	std::vector<SceneRow> rows_;

	/* Sources the user has asked us to look after, and the size each one had
	 * the last time we saw it. */
	std::map<std::string, autoscene::FitMode> managed_;
	std::map<std::string, autoscene::Watch> watches_;

	std::vector<SnapshotEntry> undo_;

	std::string sceneName_;
	int canvasW_ = 0;
	int canvasH_ = 0;

	QString lastAction_;
	bool detached_ = false;
};
