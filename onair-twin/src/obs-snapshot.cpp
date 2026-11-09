/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include "obs-snapshot.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr size_t MAX_SCENE_ITEMS = 10000;
constexpr size_t MAX_SCENE_DEPTH = 32;

std::string stringOrEmpty(const char *text)
{
	return text ? std::string(text) : std::string();
}

struct HeldItems {
	explicit HeldItems(size_t limit) : limit(limit) {}
	std::vector<obs_sceneitem_t *> items;
	size_t limit;
	bool truncated = false;
	~HeldItems()
	{
		for (obs_sceneitem_t *item : items)
			obs_sceneitem_release(item);
	}
};

bool holdItem(obs_scene_t *, obs_sceneitem_t *item, void *data)
{
	auto *items = static_cast<HeldItems *>(data);
	if (items->items.size() >= items->limit) {
		items->truncated = true;
		return false;
	}
	obs_sceneitem_addref(item);
	items->items.push_back(item);
	return true;
}

struct Scanner {
	twin::Snapshot snapshot;
	std::map<std::string, size_t> sourceIndexes;
	size_t itemCount = 0;

	void incomplete(const std::string &reason)
	{
		snapshot.complete = false;
		if (std::find(snapshot.notices.begin(), snapshot.notices.end(), reason) == snapshot.notices.end())
			snapshot.notices.push_back(reason);
	}

	std::string collectSource(obs_source_t *source, bool container)
	{
		if (!source) {
			incomplete("A source disappeared during scanning. Scan again when the setup is stable.");
			return {};
		}
		twin::Source value;
		value.id = stringOrEmpty(obs_source_get_uuid(source));
		value.name = stringOrEmpty(obs_source_get_name(source));
		value.type = stringOrEmpty(obs_source_get_id(source));
		value.container = container;
		const uint32_t flags = obs_source_get_output_flags(source);
		value.audio = (flags & OBS_SOURCE_AUDIO) != 0;
		value.video = (flags & OBS_SOURCE_VIDEO) != 0;
		value.muted = value.audio && obs_source_muted(source);
		if (value.id.empty()) {
			incomplete("OBS did not provide a stable source identity. Scan again.");
			return {};
		}
		if (value.name.empty())
			value.name = "(unnamed source)";

		auto found = sourceIndexes.find(value.id);
		if (found == sourceIndexes.end()) {
			sourceIndexes[value.id] = snapshot.sources.size();
			snapshot.sources.push_back(value);
		} else {
			const twin::Source &prior = snapshot.sources[found->second];
			if (prior.name != value.name || prior.type != value.type || prior.audio != value.audio ||
			    prior.video != value.video || prior.muted != value.muted ||
			    prior.container != value.container)
				incomplete(
					"Source settings changed during the scan. Scan again when the setup is stable.");
		}
		return value.id;
	}

	void walk(obs_scene_t *scene, twin::Scene &root, const std::string &parentKey, const std::string &parentPath,
		  bool ancestorsEnabled, std::vector<obs_scene_t *> &ancestors)
	{
		if (!scene)
			return;
		if (ancestors.size() >= MAX_SCENE_DEPTH ||
		    std::find(ancestors.begin(), ancestors.end(), scene) != ancestors.end()) {
			incomplete("A nested scene loop or depth limit prevented a complete scan.");
			return;
		}
		ancestors.push_back(scene);
		HeldItems held(itemCount < MAX_SCENE_ITEMS ? MAX_SCENE_ITEMS - itemCount : 0);
		obs_scene_enum_items(scene, holdItem, &held);
		if (held.truncated)
			incomplete("The scene-item limit was reached; this setup is too large for a complete scan.");
		for (obs_sceneitem_t *item : held.items) {
			if (++itemCount > MAX_SCENE_ITEMS) {
				incomplete(
					"The scene-item limit was reached; this setup is too large for a complete scan.");
				break;
			}
			obs_source_t *source = obs_sceneitem_get_source(item);
			if (!source) {
				incomplete("A scene item disappeared during scanning. Scan again.");
				continue;
			}

			obs_scene_t *child = obs_sceneitem_is_group(item) ? obs_sceneitem_group_get_scene(item)
									  : obs_scene_from_source(source);
			if (obs_sceneitem_is_group(item) && !child) {
				incomplete("A group could not be inspected. Scan again.");
				continue;
			}
			const bool container = child != nullptr;
			const std::string sourceId = collectSource(source, container);
			if (sourceId.empty())
				continue;
			const std::string label = stringOrEmpty(obs_source_get_name(source));
			const std::string path = parentPath.empty() ? label : parentPath + " / " + label;
			const std::string key = parentKey + "/" + std::to_string(obs_sceneitem_get_id(item));
			const bool enabled = ancestorsEnabled && obs_sceneitem_visible(item);
			root.items.push_back({key, sourceId, path, enabled});
			if (child)
				walk(child, root, key, path, enabled, ancestors);
		}
		ancestors.pop_back();
	}
};
} // namespace

twin::Snapshot captureObsSnapshot(const std::string &capturedAtUtc)
{
	Scanner scanner;
	scanner.snapshot.capturedAt = capturedAtUtc;

	char *collection = obs_frontend_get_current_scene_collection();
	scanner.snapshot.collection = stringOrEmpty(collection);
	bfree(collection);

	obs_source_t *current = obs_frontend_get_current_scene();
	if (current) {
		scanner.snapshot.currentSceneId = stringOrEmpty(obs_source_get_uuid(current));
		obs_source_release(current);
	}

	obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	if (list.sources.num == 0)
		scanner.incomplete("OBS has not supplied any scenes yet. Wait for loading to finish and scan again.");
	for (size_t i = 0; i < list.sources.num; ++i) {
		obs_source_t *source = list.sources.array[i];
		obs_scene_t *scene = source ? obs_scene_from_source(source) : nullptr;
		if (!scene) {
			scanner.incomplete("A scene disappeared during scanning. Scan again.");
			continue;
		}
		twin::Scene root;
		root.id = stringOrEmpty(obs_source_get_uuid(source));
		root.name = stringOrEmpty(obs_source_get_name(source));
		if (root.id.empty()) {
			scanner.incomplete("OBS did not provide a stable scene identity. Scan again.");
			continue;
		}
		if (root.name.empty())
			root.name = "(unnamed scene)";
		std::vector<obs_scene_t *> ancestors;
		scanner.walk(scene, root, root.id, "", true, ancestors);
		scanner.snapshot.scenes.push_back(std::move(root));
	}
	obs_frontend_source_list_free(&list);

	/* Channel zero is the video program. Other output channels may be global
	 * audio devices. This reports configured channels, not audible signal or
	 * routing to an output track. */
	for (uint32_t channel = 1; channel < MAX_CHANNELS; ++channel) {
		obs_source_t *source = obs_get_output_source(channel);
		if (!source)
			continue;
		const uint32_t flags = obs_source_get_output_flags(source);
		if ((flags & OBS_SOURCE_AUDIO) != 0) {
			const bool container = obs_group_or_scene_from_source(source) != nullptr;
			const std::string id = scanner.collectSource(source, container);
			if (!id.empty())
				scanner.snapshot.globals.push_back({channel, id});
		}
		obs_source_release(source);
	}

	return scanner.snapshot;
}
