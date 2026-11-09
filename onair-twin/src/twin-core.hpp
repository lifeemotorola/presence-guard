/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#pragma once

#include <string>
#include <vector>

/* This model contains OBS configuration metadata, never source settings,
 * stream keys, URLs, media paths, audio samples, or video frames. It is a
 * what-if model, not a model of the rendered output or hardware health. */
namespace twin {

struct Source {
	std::string id;
	std::string name;
	std::string type;
	bool audio = false;
	bool video = false;
	bool muted = false;
	bool container = false;
};

/* A flattened occurrence below one root scene, with every parent item's
 * visibility taken into account. key is made from scene/item UUIDs and IDs,
 * not names; a rename must not look like removal followed by an addition. */
struct Occurrence {
	std::string key;
	std::string sourceId;
	std::string path;
	bool enabled = false;
};

struct Scene {
	std::string id;
	std::string name;
	std::vector<Occurrence> items;
};

struct GlobalAudio {
	unsigned int channel = 0;
	std::string sourceId;
};

struct Snapshot {
	std::string collection;
	std::string capturedAt;
	std::string currentSceneId;
	bool complete = true;
	std::vector<std::string> notices;
	std::vector<Source> sources;
	std::vector<Scene> scenes;
	std::vector<GlobalAudio> globals;
};

struct Counts {
	int enabled = 0;
	int disabled = 0;
};

struct SceneImpact {
	std::string sceneId;
	std::string sceneName;
	bool current = false;
	Counts primary;
	Counts backup;
};

struct Simulation {
	bool ok = false;
	std::string error;
	Source primary;
	bool hasBackup = false;
	Source backup;
	std::vector<SceneImpact> scenes;
	std::vector<unsigned int> primaryGlobalChannels;
	std::vector<unsigned int> backupGlobalChannels;
};

struct Change {
	std::string scope;
	std::string message;
};

struct Comparison {
	bool ok = false;
	std::string error;
	std::vector<Change> changes;
};

/* Validate both captured and deserialized data. A partial/inconsistent model
 * must not yield a deceptively reassuring simulation or comparison. */
std::string validate(const Snapshot &snapshot);
const Source *findSource(const Snapshot &snapshot, const std::string &id);
Simulation simulate(const Snapshot &snapshot, const std::string &primaryId, const std::string &backupId);
Comparison compare(const Snapshot &baseline, const Snapshot &current);

} // namespace twin
