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

#include "scene-adjust.hpp"

#include <plugin-support.h>
#include <util/platform.h>

#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <utility>

/* How often the scene is re-read. Sources change size without telling anyone,
 * so asking three times a second is cheap insurance. */
static const int POLL_INTERVAL_MS = 700;

/* One step of undo, capped: there is no reason to remember more. */
static const size_t MAX_UNDO = 500;

static const char *SETTINGS_FILE = "settings.json";

static std::string safeStr(const char *value)
{
	return value ? std::string(value) : std::string();
}

/* Holds the reference obs_frontend_get_current_scene() hands out. */
namespace {
class SceneHandle {
public:
	explicit SceneHandle(obs_source_t *source) : source_(source)
	{
		if (source_)
			scene_ = obs_scene_from_source(source_);
	}

	~SceneHandle()
	{
		if (source_)
			obs_source_release(source_);
	}

	SceneHandle(const SceneHandle &) = delete;
	SceneHandle &operator=(const SceneHandle &) = delete;

	obs_scene_t *scene() const { return scene_; }
	explicit operator bool() const { return scene_ != nullptr; }

private:
	obs_source_t *source_ = nullptr;
	obs_scene_t *scene_ = nullptr;
};

bool collectItem(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);
	auto *items = static_cast<std::vector<obs_sceneitem_t *> *>(param);
	obs_sceneitem_addref(item);
	items->push_back(item);
	return true;
}

/* Scene items are not safe to walk while changing them, so they are collected
 * with a reference each and released afterwards. */
void releaseItems(std::vector<obs_sceneitem_t *> &items)
{
	for (obs_sceneitem_t *item : items)
		obs_sceneitem_release(item);
	items.clear();
}
} // namespace

SceneAdjust::SceneAdjust(QObject *parent) : QObject(parent)
{
	QTimer *timer = new QTimer(this);
	timer->setInterval(POLL_INTERVAL_MS);
	QObject::connect(timer, &QTimer::timeout, [this]() { tick(); });
	timer->start();
}

SceneAdjust::~SceneAdjust() = default;

void SceneAdjust::tick()
{
	if (detached_)
		return;

	refresh();
}

/* ------------------------------------------------------------------------- */
/* reading the scene                                                         */
/* ------------------------------------------------------------------------- */

std::string SceneAdjust::keyFor(const std::string &scene, int64_t id)
{
	return scene + "#" + std::to_string(id);
}

autoscene::FitMode SceneAdjust::managedMode(const std::string &scene, int64_t id) const
{
	auto it = managed_.find(keyFor(scene, id));
	return it == managed_.end() ? autoscene::FitMode::None : it->second;
}

/* OBS keeps the item position at its alignment anchor. The maths wants the
 * top-left corner, so the anchor is taken out here and put back on apply. */
autoscene::Item SceneAdjust::measure(const obs_transform_info &info, int srcW, int srcH)
{
	autoscene::Item item;
	item.srcW = srcW;
	item.srcH = srcH;
	item.scaleX = info.scale.x;
	item.scaleY = info.scale.y;
	item.rotation = static_cast<int>(std::lround(info.rot));

	const double drawnW = static_cast<double>(srcW) * static_cast<double>(info.scale.x);
	const double drawnH = static_cast<double>(srcH) * static_cast<double>(info.scale.y);

	double x = static_cast<double>(info.pos.x);
	double y = static_cast<double>(info.pos.y);

	if (info.alignment & OBS_ALIGN_RIGHT)
		x -= drawnW;
	else if (!(info.alignment & OBS_ALIGN_LEFT))
		x -= drawnW / 2.0;

	if (info.alignment & OBS_ALIGN_BOTTOM)
		y -= drawnH;
	else if (!(info.alignment & OBS_ALIGN_TOP))
		y -= drawnH / 2.0;

	item.posX = x;
	item.posY = y;
	return item;
}

void SceneAdjust::refreshRows()
{
	rows_.clear();

	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		canvasW_ = static_cast<int>(ovi.base_width);
		canvasH_ = static_cast<int>(ovi.base_height);
	}

	obs_source_t *source = obs_frontend_get_current_scene();
	if (!source) {
		sceneName_.clear();
		return;
	}

	sceneName_ = safeStr(obs_source_get_name(source));
	obs_scene_t *scene = obs_scene_from_source(source);

	std::vector<obs_sceneitem_t *> items;
	if (scene)
		obs_scene_enum_items(scene, collectItem, &items);

	for (obs_sceneitem_t *item : items) {
		obs_source_t *itemSource = obs_sceneitem_get_source(item);

		SceneRow row;
		row.id = obs_sceneitem_get_id(item);
		row.name = safeStr(obs_source_get_name(itemSource));
		row.srcW = static_cast<int>(obs_source_get_width(itemSource));
		row.srcH = static_cast<int>(obs_source_get_height(itemSource));
		row.locked = obs_sceneitem_locked(item);
		row.group = obs_sceneitem_is_group(item);
		row.visible = obs_sceneitem_visible(item);
		row.mode = managedMode(sceneName_, row.id);

		struct obs_transform_info info;
		obs_sceneitem_get_info2(item, &info);

		autoscene::AuditOptions options;
		options.adopted = row.mode;
		const autoscene::Report report =
			autoscene::audit(measure(info, row.srcW, row.srcH), canvasW_, canvasH_, options);

		row.rect = report.rect;
		row.issue = report.issue;
		rows_.push_back(row);
	}

	releaseItems(items);
	obs_source_release(source);
}

void SceneAdjust::refresh()
{
	const int previousW = canvasW_;
	const int previousH = canvasH_;

	refreshRows();

	/* libobs has no "the canvas changed" frontend event, so the size is
	 * compared here instead: it is the one change that invalidates every
	 * adopted source at once. */
	const bool canvasChanged = previousW > 0 && previousH > 0 && (previousW != canvasW_ || previousH != canvasH_);

	if (canvasChanged) {
		refitEverything();
		refreshRows();
	} else {
		maybeAutoRefit();
	}
}

int SceneAdjust::problemCount() const
{
	int count = 0;
	for (const SceneRow &row : rows_)
		if (row.issue != autoscene::Issue::Fine && row.issue != autoscene::Issue::NoSize)
			count++;
	return count;
}

int SceneAdjust::adjustableCount() const
{
	int count = 0;
	for (const SceneRow &row : rows_)
		if (row.canAdjust())
			count++;
	return count;
}

/* ------------------------------------------------------------------------- */
/* the automatic half                                                        */
/* ------------------------------------------------------------------------- */

void SceneAdjust::maybeAutoRefit()
{
	std::vector<std::pair<int64_t, autoscene::FitMode>> due;

	for (const SceneRow &row : rows_) {
		const std::string key = keyFor(sceneName_, row.id);

		if (row.mode == autoscene::FitMode::None) {
			watches_.erase(key);
			continue;
		}

		const autoscene::Watch now{row.srcW, row.srcH, canvasW_, canvasH_};
		auto it = watches_.find(key);
		if (it == watches_.end()) {
			/* First sight in this session: remember, do not move anything.
			 * Opening a scene must never rearrange it. */
			watches_[key] = now;
			continue;
		}

		const bool changed = autoscene::needsRefit(row.mode, it->second, now);
		it->second = now;

		if (!config.autoRefit || !row.canAdjust())
			continue;

		if (changed)
			due.emplace_back(row.id, row.mode);
	}

	if (due.empty())
		return;

	snapshotScene(sceneName_);

	int fixed = 0;
	for (const auto &entry : due)
		if (applyMode(entry.first, entry.second, false))
			fixed++;

	if (fixed > 0) {
		setMessage("AutoScene.Message.AutoFixed", fixed);
		obs_log(LOG_INFO, "re-fitted %d source(s) in '%s' after a size change", fixed, sceneName_.c_str());
		refreshRows();
	}
}

/* Canvas change: every scene is affected, not just the one on screen. */
void SceneAdjust::refitEverything()
{
	if (!config.autoRefit || managed_.empty())
		return;

	struct obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);

	snapshotAll();

	int fixed = 0;

	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *source = list.sources.array[i];
		obs_scene_t *scene = obs_scene_from_source(source);
		if (!scene)
			continue;

		const std::string sceneName = safeStr(obs_source_get_name(source));

		std::vector<obs_sceneitem_t *> items;
		obs_scene_enum_items(scene, collectItem, &items);

		for (obs_sceneitem_t *item : items) {
			const int64_t id = obs_sceneitem_get_id(item);
			const autoscene::FitMode mode = managedMode(sceneName, id);
			if (mode == autoscene::FitMode::None)
				continue;

			if (obs_sceneitem_locked(item) || obs_sceneitem_is_group(item))
				continue;

			obs_source_t *itemSource = obs_sceneitem_get_source(item);
			const int srcW = static_cast<int>(obs_source_get_width(itemSource));
			const int srcH = static_cast<int>(obs_source_get_height(itemSource));
			if (srcW <= 0 || srcH <= 0)
				continue;

			autoscene::PlacementOptions options;
			options.margin = config.margin;
			options.align = config.align;
			const autoscene::Placement placement =
				autoscene::place(mode, srcW, srcH, canvasW_, canvasH_, options);

			if (placeItem(item, placement))
				fixed++;

			watches_[keyFor(sceneName, id)] = autoscene::Watch{srcW, srcH, canvasW_, canvasH_};
		}

		releaseItems(items);
	}

	obs_frontend_source_list_free(&list);

	if (fixed > 0) {
		setMessage("AutoScene.Message.AutoFixed", fixed);
		obs_log(LOG_INFO, "re-fitted %d source(s) across all scenes after a canvas change", fixed);
	}
}

/* ------------------------------------------------------------------------- */
/* applying a placement                                                      */
/* ------------------------------------------------------------------------- */

bool SceneAdjust::placeItem(obs_sceneitem_t *item, const autoscene::Placement &placement)
{
	if (!item || !placement.ok)
		return false;

	if (obs_sceneitem_locked(item))
		return false;

	struct obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);

	info.pos.x = static_cast<float>(placement.posX);
	info.pos.y = static_cast<float>(placement.posY);
	info.scale.x = static_cast<float>(placement.scaleX);
	info.scale.y = static_cast<float>(placement.scaleY);
	/* The maths works in top-left corners. */
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	/* A bounding box would override the scale we just computed, so the item is
	 * taken back to a plain transform. */
	info.bounds_type = OBS_BOUNDS_NONE;

	obs_sceneitem_defer_update_begin(item);
	obs_sceneitem_set_info2(item, &info);
	obs_sceneitem_defer_update_end(item);
	return true;
}

bool SceneAdjust::applyPlacement(int64_t id, autoscene::FitMode mode, const autoscene::Rect *cell)
{
	SceneHandle scene(obs_frontend_get_current_scene());
	if (!scene)
		return false;

	bool applied = false;

	std::vector<obs_sceneitem_t *> items;
	obs_scene_enum_items(scene.scene(), collectItem, &items);

	for (obs_sceneitem_t *item : items) {
		if (obs_sceneitem_get_id(item) != id)
			continue;

		obs_source_t *itemSource = obs_sceneitem_get_source(item);
		const int srcW = static_cast<int>(obs_source_get_width(itemSource));
		const int srcH = static_cast<int>(obs_source_get_height(itemSource));

		autoscene::Placement placement;
		if (cell) {
			placement = autoscene::placeInBox(mode, srcW, srcH, *cell, autoscene::Align::Center);
		} else {
			autoscene::PlacementOptions options;
			options.margin = config.margin;
			options.align = config.align;
			placement = autoscene::place(mode, srcW, srcH, canvasW_, canvasH_, options);
		}

		applied = placeItem(item, placement);
		break;
	}

	releaseItems(items);
	return applied;
}

bool SceneAdjust::applyMode(int64_t id, autoscene::FitMode mode, bool report)
{
	if (mode == autoscene::FitMode::None)
		return false;

	const bool applied = applyPlacement(id, mode, nullptr);
	if (report) {
		if (applied)
			setMessage("AutoScene.Message.Applied");
		else
			setMessage("AutoScene.Message.Locked");
	}
	return applied;
}

/* ------------------------------------------------------------------------- */
/* the dock's buttons                                                        */
/* ------------------------------------------------------------------------- */

void SceneAdjust::setMode(int64_t id, autoscene::FitMode mode)
{
	if (sceneName_.empty())
		return;

	refreshRows();

	const std::string key = keyFor(sceneName_, id);

	if (mode == autoscene::FitMode::None) {
		managed_.erase(key);
		watches_.erase(key);
		setMessage("AutoScene.Message.Released");
		saveNow();
		refreshRows();
		return;
	}

	snapshotScene(sceneName_);

	managed_[key] = mode;

	bool applied = false;
	for (const SceneRow &row : rows_) {
		if (row.id != id)
			continue;
		if (!row.canAdjust()) {
			setMessage("AutoScene.Message.Locked");
			break;
		}
		applied = applyPlacement(id, mode, nullptr);
		break;
	}

	if (applied)
		setMessage("AutoScene.Message.Adopted");

	saveNow();
	refreshRows();
}

void SceneAdjust::fixRow(int64_t id)
{
	refreshRows();

	for (const SceneRow &row : rows_) {
		if (row.id != id)
			continue;

		const autoscene::FitMode mode = row.mode != autoscene::FitMode::None ? row.mode
										     : autoscene::FitMode::Fit;

		if (!row.canAdjust()) {
			setMessage("AutoScene.Message.Locked");
			return;
		}

		snapshotScene(sceneName_);
		managed_[keyFor(sceneName_, id)] = mode;

		if (applyPlacement(id, mode, nullptr))
			setMessage("AutoScene.Message.Applied");
		else
			setMessage("AutoScene.Message.Locked");

		saveNow();
		refreshRows();
		return;
	}
}

void SceneAdjust::fixScene()
{
	refreshRows();

	if (sceneName_.empty()) {
		setMessage("AutoScene.Message.NoScene");
		return;
	}

	std::vector<int64_t> ids;
	for (const SceneRow &row : rows_)
		if (row.canAdjust())
			ids.push_back(row.id);

	if (ids.empty()) {
		setMessage("AutoScene.Message.Nothing");
		return;
	}

	snapshotScene(sceneName_);

	int fixed = 0;
	for (int64_t id : ids) {
		managed_[keyFor(sceneName_, id)] = autoscene::FitMode::Fit;
		if (applyPlacement(id, autoscene::FitMode::Fit, nullptr))
			fixed++;
	}

	saveNow();
	refreshRows();

	if (fixed > 0) {
		setMessage("AutoScene.Message.Fixed", fixed);
		obs_log(LOG_INFO, "fitted %d source(s) in '%s'", fixed, sceneName_.c_str());
	} else {
		setMessage("AutoScene.Message.Locked");
	}
}

void SceneAdjust::applyLayout(autoscene::Layout layout)
{
	if (layout == autoscene::Layout::None)
		return;

	refreshRows();

	if (sceneName_.empty()) {
		setMessage("AutoScene.Message.NoScene");
		return;
	}

	std::vector<int64_t> ids;
	for (const SceneRow &row : rows_)
		if (row.canAdjust())
			ids.push_back(row.id);

	if (ids.empty()) {
		setMessage("AutoScene.Message.Nothing");
		return;
	}

	const std::vector<autoscene::Rect> cells =
		autoscene::layoutCells(layout, static_cast<int>(ids.size()), canvasW_, canvasH_, config.gap);

	if (cells.size() != ids.size()) {
		setMessage("AutoScene.Message.Nothing");
		return;
	}

	snapshotScene(sceneName_);

	int placed = 0;
	for (size_t i = 0; i < ids.size(); i++)
		if (applyPlacement(ids[i], autoscene::FitMode::Fit, &cells[i]))
			placed++;

	refreshRows();

	if (placed > 0) {
		setMessage("AutoScene.Message.Layout", placed);
		obs_log(LOG_INFO, "laid %d source(s) out as '%s' in '%s'", placed, autoscene::layoutName(layout),
			sceneName_.c_str());
	}
}

void SceneAdjust::undo()
{
	if (undo_.empty())
		return;

	struct obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);

	int restored = 0;

	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *source = list.sources.array[i];
		obs_scene_t *scene = obs_scene_from_source(source);
		if (!scene)
			continue;

		const std::string sceneName = safeStr(obs_source_get_name(source));

		std::vector<obs_sceneitem_t *> items;
		obs_scene_enum_items(scene, collectItem, &items);

		for (obs_sceneitem_t *item : items) {
			const int64_t id = obs_sceneitem_get_id(item);

			for (const SnapshotEntry &entry : undo_) {
				if (entry.scene != sceneName || entry.id != id)
					continue;

				obs_sceneitem_defer_update_begin(item);
				obs_sceneitem_set_info2(item, &entry.info);
				obs_sceneitem_defer_update_end(item);
				restored++;
				break;
			}
		}

		releaseItems(items);
	}

	obs_frontend_source_list_free(&list);

	undo_.clear();

	setMessage("AutoScene.Message.Undone");
	obs_log(LOG_INFO, "undid the last change to %d source(s)", restored);
	refreshRows();
}

/* ------------------------------------------------------------------------- */
/* one step of undo                                                          */
/* ------------------------------------------------------------------------- */

void SceneAdjust::snapshotScene(const std::string &sceneName)
{
	undo_.clear();

	SceneHandle scene(obs_frontend_get_current_scene());
	if (!scene || sceneName.empty())
		return;

	std::vector<obs_sceneitem_t *> items;
	obs_scene_enum_items(scene.scene(), collectItem, &items);

	for (obs_sceneitem_t *item : items) {
		if (undo_.size() >= MAX_UNDO)
			break;

		SnapshotEntry entry;
		entry.scene = sceneName;
		entry.id = obs_sceneitem_get_id(item);
		obs_sceneitem_get_info2(item, &entry.info);
		undo_.push_back(entry);
	}

	releaseItems(items);
}

void SceneAdjust::snapshotAll()
{
	undo_.clear();

	struct obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);

	for (size_t i = 0; i < list.sources.num && undo_.size() < MAX_UNDO; i++) {
		obs_source_t *source = list.sources.array[i];
		obs_scene_t *scene = obs_scene_from_source(source);
		if (!scene)
			continue;

		const std::string sceneName = safeStr(obs_source_get_name(source));

		std::vector<obs_sceneitem_t *> items;
		obs_scene_enum_items(scene, collectItem, &items);

		for (obs_sceneitem_t *item : items) {
			if (undo_.size() >= MAX_UNDO)
				break;

			SnapshotEntry entry;
			entry.scene = sceneName;
			entry.id = obs_sceneitem_get_id(item);
			obs_sceneitem_get_info2(item, &entry.info);
			undo_.push_back(entry);
		}

		releaseItems(items);
	}

	obs_frontend_source_list_free(&list);
}

/* ------------------------------------------------------------------------- */
/* settings                                                                  */
/* ------------------------------------------------------------------------- */

void SceneAdjust::setMessage(const char *key, int count)
{
	const char *text = obs_module_text(key);
	if (!text || !*text)
		text = key;

	lastAction_ = QString::fromUtf8(text);
	if (count >= 0)
		lastAction_ = lastAction_.arg(count);
}

QString SceneAdjust::sceneText() const
{
	if (sceneName_.empty())
		return QString::fromUtf8(obs_module_text("AutoScene.Status.NoScene"));

	QString text = QString::fromUtf8(obs_module_text("AutoScene.Status.Scene"));
	text = text.arg(QString::fromStdString(sceneName_));
	return text.arg(canvasW_).arg(canvasH_);
}

QString SceneAdjust::summaryText() const
{
	QString text = QString::fromUtf8(obs_module_text("AutoScene.Status.Summary"));
	text = text.arg(adjustableCount()).arg(problemCount());

	if (!lastAction_.isEmpty())
		return text + QStringLiteral(" · ") + lastAction_;
	return text;
}

void SceneAdjust::setAutoRefit(bool on)
{
	if (config.autoRefit == on)
		return;

	config.autoRefit = on;
	saveNow();
}

void SceneAdjust::setMargin(int margin)
{
	const int next = std::max(0, std::min(500, margin));
	if (next == config.margin)
		return;

	config.margin = next;
	saveNow();
}

void SceneAdjust::setGap(int gap)
{
	const int next = std::max(0, std::min(200, gap));
	if (next == config.gap)
		return;

	config.gap = next;
	saveNow();
}

void SceneAdjust::setAlign(autoscene::Align align)
{
	if (align == config.align)
		return;

	config.align = align;
	saveNow();
}

void SceneAdjust::loadSettings()
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

	if (obs_data_has_user_value(data, "autoRefit"))
		config.autoRefit = obs_data_get_bool(data, "autoRefit");
	if (obs_data_has_user_value(data, "margin"))
		config.margin = static_cast<int>(obs_data_get_int(data, "margin"));
	if (obs_data_has_user_value(data, "gap"))
		config.gap = static_cast<int>(obs_data_get_int(data, "gap"));
	if (obs_data_has_user_value(data, "align"))
		config.align = autoscene::alignFromIndex(static_cast<int>(obs_data_get_int(data, "align")));

	managed_.clear();
	obs_data_array_t *array = obs_data_get_array(data, "managed");
	if (array) {
		const size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(array, i);
			if (!item)
				continue;

			const std::string scene = safeStr(obs_data_get_string(item, "scene"));
			const int64_t id = obs_data_get_int(item, "id");
			const autoscene::FitMode mode =
				autoscene::fitModeFromName(safeStr(obs_data_get_string(item, "mode")));
			if (!scene.empty() && mode != autoscene::FitMode::None)
				managed_[keyFor(scene, id)] = mode;

			obs_data_release(item);
		}
		obs_data_array_release(array);
	}

	obs_data_release(data);
}

void SceneAdjust::saveNow() const
{
	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "autoRefit", config.autoRefit);
	obs_data_set_int(data, "margin", config.margin);
	obs_data_set_int(data, "gap", config.gap);
	obs_data_set_int(data, "align", autoscene::alignIndex(config.align));

	obs_data_array_t *array = obs_data_array_create();
	for (const auto &entry : managed_) {
		/* The key is "scene#id"; the id is the last part. */
		const std::string &key = entry.first;
		const size_t hash = key.rfind('#');
		if (hash == std::string::npos)
			continue;

		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "scene", key.substr(0, hash).c_str());
		obs_data_set_int(item, "id", strtoll(key.c_str() + hash + 1, nullptr, 10));
		obs_data_set_string(item, "mode", autoscene::fitModeName(entry.second));
		obs_data_array_push_back(array, item);
		obs_data_release(item);
	}
	obs_data_set_array(data, "managed", array);
	obs_data_array_release(array);

	char *path = obs_module_config_path(SETTINGS_FILE);
	if (path) {
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
/* frontend events                                                           */
/* ------------------------------------------------------------------------- */

void SceneAdjust::handleEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		loadSettings();
		refresh();
		break;

	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		refresh();
		break;

	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		refresh();
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		/* Item ids mean nothing in a different collection. */
		watches_.clear();
		undo_.clear();
		refresh();
		break;

	default:
		break;
	}
}
