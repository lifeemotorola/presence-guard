/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include "twin-core.hpp"

#include <map>
#include <set>

namespace twin {

const Source *findSource(const Snapshot &snapshot, const std::string &id)
{
	for (const Source &source : snapshot.sources)
		if (source.id == id)
			return &source;
	return nullptr;
}

std::string validate(const Snapshot &snapshot)
{
	if (!snapshot.complete)
		return "The scan was incomplete. Scan again before running a simulation or comparison.";
	if (snapshot.collection.empty())
		return "The scene collection name is unavailable. Wait for OBS to finish loading, then scan again.";

	std::set<std::string> sources;
	for (const Source &source : snapshot.sources) {
		if (source.id.empty() || !sources.insert(source.id).second)
			return "The snapshot contains a missing or duplicate source identity.";
	}

	std::set<std::string> scenes;
	for (const Scene &scene : snapshot.scenes) {
		if (scene.id.empty() || !scenes.insert(scene.id).second)
			return "The snapshot contains a missing or duplicate scene identity.";
		std::set<std::string> keys;
		for (const Occurrence &item : scene.items) {
			if (item.key.empty() || !keys.insert(item.key).second || sources.count(item.sourceId) == 0)
				return "The snapshot contains a missing, duplicate or unresolved scene item.";
		}
	}

	std::set<unsigned int> channels;
	for (const GlobalAudio &global : snapshot.globals) {
		if (global.channel == 0 || global.channel >= 64 || !channels.insert(global.channel).second ||
		    sources.count(global.sourceId) == 0)
			return "The snapshot contains an invalid global-audio channel.";
	}
	return {};
}

Simulation simulate(const Snapshot &snapshot, const std::string &primaryId, const std::string &backupId)
{
	Simulation result;
	result.error = validate(snapshot);
	if (!result.error.empty())
		return result;

	const Source *primary = findSource(snapshot, primaryId);
	if (!primary || primary->container) {
		result.error = "Choose a source in the scanned scene collection.";
		return result;
	}
	result.primary = *primary;

	if (!backupId.empty()) {
		const Source *backup = findSource(snapshot, backupId);
		if (!backup || backup->container) {
			result.error = "The selected backup is not in the scanned scene collection.";
			return result;
		}
		if (primaryId == backupId) {
			result.error = "The backup must be a different source from the simulated failure.";
			return result;
		}
		result.hasBackup = true;
		result.backup = *backup;
	}

	for (const Scene &scene : snapshot.scenes) {
		SceneImpact impact;
		impact.sceneId = scene.id;
		impact.sceneName = scene.name;
		impact.current = !snapshot.currentSceneId.empty() && scene.id == snapshot.currentSceneId;
		for (const Occurrence &item : scene.items) {
			Counts *counts = nullptr;
			if (item.sourceId == primaryId)
				counts = &impact.primary;
			else if (result.hasBackup && item.sourceId == backupId)
				counts = &impact.backup;
			if (counts) {
				if (item.enabled)
					counts->enabled++;
				else
					counts->disabled++;
			}
		}
		if (impact.primary.enabled || impact.primary.disabled)
			result.scenes.push_back(impact);
	}

	for (const GlobalAudio &global : snapshot.globals) {
		if (global.sourceId == primaryId)
			result.primaryGlobalChannels.push_back(global.channel);
		if (result.hasBackup && global.sourceId == backupId)
			result.backupGlobalChannels.push_back(global.channel);
	}

	result.ok = true;
	return result;
}

template<typename T> static std::map<std::string, const T *> indexById(const std::vector<T> &values)
{
	std::map<std::string, const T *> result;
	for (const T &value : values)
		result[value.id] = &value;
	return result;
}

static std::string sourceName(const Snapshot &snapshot, const std::string &id)
{
	const Source *source = findSource(snapshot, id);
	return source ? source->name : id;
}

Comparison compare(const Snapshot &baseline, const Snapshot &current)
{
	Comparison result;
	result.error = validate(baseline);
	if (!result.error.empty())
		return result;
	result.error = validate(current);
	if (!result.error.empty())
		return result;
	if (baseline.collection != current.collection) {
		result.error =
			"The saved baseline belongs to a different scene collection. Switch back or save a new baseline.";
		return result;
	}

	auto oldSources = indexById(baseline.sources);
	auto newSources = indexById(current.sources);
	for (const auto &entry : oldSources) {
		const Source &old = *entry.second;
		auto found = newSources.find(entry.first);
		if (found == newSources.end()) {
			result.changes.push_back({"Sources", "Removed: " + old.name});
			continue;
		}
		const Source &now = *found->second;
		if (old.name != now.name)
			result.changes.push_back({"Sources", "Renamed: " + old.name + " → " + now.name});
		if (old.type != now.type || old.audio != now.audio || old.video != now.video ||
		    old.container != now.container)
			result.changes.push_back({"Sources", "Type or capabilities changed: " + now.name});
		if ((old.audio || now.audio) && old.muted != now.muted)
			result.changes.push_back({"Audio", now.name + (now.muted ? " is muted" : " is unmuted")});
	}
	for (const auto &entry : newSources)
		if (!oldSources.count(entry.first))
			result.changes.push_back({"Sources", "Added: " + entry.second->name});

	auto oldScenes = indexById(baseline.scenes);
	auto newScenes = indexById(current.scenes);
	for (const auto &entry : oldScenes) {
		const Scene &old = *entry.second;
		auto found = newScenes.find(entry.first);
		if (found == newScenes.end()) {
			result.changes.push_back({"Scenes", "Removed scene: " + old.name});
			continue;
		}
		const Scene &now = *found->second;
		if (old.name != now.name)
			result.changes.push_back({"Scenes", "Renamed scene: " + old.name + " → " + now.name});

		std::map<std::string, const Occurrence *> oldItems;
		std::map<std::string, const Occurrence *> newItems;
		for (const Occurrence &item : old.items)
			oldItems[item.key] = &item;
		for (const Occurrence &item : now.items)
			newItems[item.key] = &item;

		for (const auto &itemEntry : oldItems) {
			const Occurrence &before = *itemEntry.second;
			auto newItem = newItems.find(itemEntry.first);
			if (newItem == newItems.end()) {
				result.changes.push_back({now.name, "Removed item: " + before.path});
				continue;
			}
			const Occurrence &after = *newItem->second;
			if (before.sourceId != after.sourceId)
				result.changes.push_back(
					{now.name, "Replaced item: " + before.path + " → " + after.path});
			else if (before.enabled != after.enabled)
				result.changes.push_back(
					{now.name, after.path + (after.enabled ? " path enabled" : " path disabled")});
		}
		for (const auto &itemEntry : newItems)
			if (!oldItems.count(itemEntry.first))
				result.changes.push_back({now.name, "Added item: " + itemEntry.second->path});
	}
	for (const auto &entry : newScenes)
		if (!oldScenes.count(entry.first))
			result.changes.push_back({"Scenes", "Added scene: " + entry.second->name});

	std::map<unsigned int, std::string> oldGlobals;
	std::map<unsigned int, std::string> newGlobals;
	for (const auto &global : baseline.globals)
		oldGlobals[global.channel] = global.sourceId;
	for (const auto &global : current.globals)
		newGlobals[global.channel] = global.sourceId;
	std::set<unsigned int> channels;
	for (const auto &entry : oldGlobals)
		channels.insert(entry.first);
	for (const auto &entry : newGlobals)
		channels.insert(entry.first);
	for (unsigned int channel : channels) {
		auto old = oldGlobals.find(channel);
		auto now = newGlobals.find(channel);
		if (old == oldGlobals.end())
			result.changes.push_back({"Global audio", "Channel " + std::to_string(channel) + " added: " +
									  sourceName(current, now->second)});
		else if (now == newGlobals.end())
			result.changes.push_back({"Global audio", "Channel " + std::to_string(channel) + " removed: " +
									  sourceName(baseline, old->second)});
		else if (old->second != now->second)
			result.changes.push_back({"Global audio", "Channel " + std::to_string(channel) + " changed: " +
									  sourceName(baseline, old->second) + " → " +
									  sourceName(current, now->second)});
	}

	result.ok = true;
	return result;
}

} // namespace twin
