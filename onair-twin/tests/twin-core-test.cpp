/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include "twin-core.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace twin;
static int checks = 0;
static int failures = 0;

static void check(bool condition, const char *description)
{
	++checks;
	if (!condition) {
		++failures;
		std::printf("FAIL  %s\n", description);
	} else {
		std::printf("PASS  %s\n", description);
	}
}

static Snapshot sample()
{
	Snapshot s;
	s.collection = "Sunday show";
	s.capturedAt = "2026-11-02T10:00:00Z";
	s.currentSceneId = "scene-live";
	s.sources = {
		{"camera-a", "Main Camera", "dshow_input", false, true, false, false},
		{"camera-b", "Backup Camera", "dshow_input", false, true, false, false},
		{"mic-a", "Host Microphone", "wasapi_input_capture", true, false, false, false},
		{"mic-b", "Backup Microphone", "wasapi_input_capture", true, false, true, false},
		{"group-1", "Stage Group", "group", false, true, false, true},
	};
	s.scenes = {
		{"scene-live",
		 "Live",
		 {
			 {"10", "camera-a", "Main Camera", true},
			 {"11", "camera-b", "Backup Camera", false},
			 {"12", "group-1", "Stage Group", true},
			 {"12/20", "mic-a", "Stage Group / Host Microphone", true},
			 {"12/21", "mic-b", "Stage Group / Backup Microphone", false},
		 }},
		{"scene-interview",
		 "Interview",
		 {
			 {"15", "camera-a", "Main Camera", false},
			 {"16", "camera-a", "Main Camera (duplicate)", true},
			 {"17", "camera-b", "Backup Camera", true},
		 }},
		{"scene-slate", "Slate", {}},
	};
	s.globals = {{1, "mic-a"}, {2, "mic-b"}};
	return s;
}

static bool hasMessage(const Comparison &comparison, const std::string &needle)
{
	for (const Change &change : comparison.changes)
		if (change.message.find(needle) != std::string::npos)
			return true;
	return false;
}

int main()
{
	const Snapshot original = sample();
	check(validate(original).empty(), "well-formed snapshot is accepted");
	check(findSource(original, "mic-a") && findSource(original, "mic-a")->audio, "find source by stable ID");
	check(findSource(original, "absent") == nullptr, "missing source is not invented");

	Simulation camera = simulate(original, "camera-a", "camera-b");
	check(camera.ok && camera.scenes.size() == 2, "camera failure finds all dependent scenes");
	check(camera.scenes[0].current && camera.scenes[0].primary.enabled == 1, "marks the current reported scene");
	check(camera.scenes[0].backup.disabled == 1 && camera.scenes[0].backup.enabled == 0,
	      "hidden backup is not reported as enabled");
	check(camera.scenes[1].primary.enabled == 1 && camera.scenes[1].primary.disabled == 1,
	      "duplicate appearances are counted independently");
	check(camera.scenes[1].backup.enabled == 1, "enabled alternate is counted in its actual scene");
	check(camera.primaryGlobalChannels.empty(), "video source is not invented as global audio");
	check(camera.backupGlobalChannels.empty(), "scene-only backup is not invented as global audio");
	check(simulate(original, "camera-a", "").ok, "backup is optional");
	check(!simulate(original, "camera-a", "camera-a").ok, "primary cannot be its own backup");
	check(!simulate(original, "camera-a", "missing").ok, "missing backup is rejected");
	check(!simulate(original, "missing", "").ok, "missing primary is rejected");
	check(!simulate(original, "group-1", "").ok, "container is not presented as a device");
	check(!simulate(original, "camera-a", "group-1").ok, "container is not presented as a backup device");

	Simulation mic = simulate(original, "mic-a", "mic-b");
	check(mic.ok && mic.scenes.size() == 1, "nested microphone is found in its root scene");
	check(mic.primaryGlobalChannels.size() == 1 && mic.primaryGlobalChannels[0] == 1,
	      "global microphone channel is recorded separately");
	check(mic.backupGlobalChannels.size() == 1 && mic.backupGlobalChannels[0] == 2,
	      "backup global channel is recorded separately");
	check(mic.hasBackup && mic.backup.muted, "muted backup is explicitly retained");
	check(mic.scenes[0].backup.disabled == 1, "nested hidden backup is counted as hidden");
	check(original.scenes[0].items[0].enabled && original.sources[3].muted,
	      "what-if simulation does not mutate the snapshot");

	Snapshot s = original;
	s.complete = false;
	check(!simulate(s, "camera-a", "").ok, "incomplete scan cannot give a simulation");
	check(!compare(original, s).ok, "incomplete scan cannot produce a comparison");
	s = original;
	s.collection.clear();
	check(!simulate(s, "camera-a", "").ok, "collection required");
	s = original;
	s.sources.push_back(s.sources.front());
	check(!validate(s).empty(), "duplicate source identity rejected");
	s = original;
	s.sources[0].id.clear();
	check(!validate(s).empty(), "empty source identity rejected");
	s = original;
	s.scenes.push_back(s.scenes.front());
	check(!validate(s).empty(), "duplicate scene identity rejected");
	s = original;
	s.scenes[0].items.push_back(s.scenes[0].items[0]);
	check(!validate(s).empty(), "duplicate item path rejected");
	s = original;
	s.scenes[0].items[0].sourceId = "does-not-exist";
	check(!validate(s).empty(), "dangling item source rejected");
	s = original;
	s.globals[0].sourceId = "does-not-exist";
	check(!validate(s).empty(), "dangling global source rejected");
	s = original;
	s.globals.push_back(s.globals[0]);
	check(!validate(s).empty(), "duplicate global channel rejected");
	s = original;
	s.globals[0].channel = 0;
	check(!validate(s).empty(), "program output channel is not global audio");

	Comparison same = compare(original, original);
	check(same.ok && same.changes.empty(), "identical snapshots have no reported changes");
	s = original;
	s.collection = "Other show";
	check(!compare(original, s).ok, "different collection baseline is never compared");
	s = original;
	s.currentSceneId = "scene-interview";
	check(compare(original, s).ok && compare(original, s).changes.empty(),
	      "operator scene switch alone is not treated as drift");
	s = original;
	s.sources[0].name = "Renamed Camera";
	Comparison renamed = compare(original, s);
	check(renamed.ok && hasMessage(renamed, "Renamed: Main Camera"),
	      "rename recorded without losing source identity");
	check(!hasMessage(renamed, "Removed: Main Camera"), "rename does not become a removal");
	s = original;
	s.scenes[0].name = "New live name";
	check(hasMessage(compare(original, s), "Renamed scene: Live"), "scene rename recorded");
	s = original;
	s.sources[2].muted = true;
	check(hasMessage(compare(original, s), "Host Microphone is muted"), "mute change recorded");
	s = original;
	s.sources[3].muted = false;
	check(hasMessage(compare(original, s), "Backup Microphone is unmuted"), "unmute change recorded");
	s = original;
	s.sources[0].type = "different";
	check(hasMessage(compare(original, s), "Type or capabilities changed"), "type change recorded");
	s = original;
	s.scenes[0].items[0].enabled = false;
	check(hasMessage(compare(original, s), "Main Camera path disabled"), "disabled path recorded");
	s = original;
	s.scenes[0].items[1].enabled = true;
	check(hasMessage(compare(original, s), "Backup Camera path enabled"), "enabled path recorded");
	s = original;
	s.scenes[0].items[0].sourceId = "camera-b";
	s.scenes[0].items[0].path = "Backup Camera";
	check(hasMessage(compare(original, s), "Replaced item"), "replacement with the same item ID is not missed");
	s = original;
	s.scenes[0].items.erase(s.scenes[0].items.begin());
	check(hasMessage(compare(original, s), "Removed item: Main Camera"), "removed occurrence recorded");
	s = original;
	s.scenes[2].items.push_back({"50", "camera-a", "Main Camera", true});
	check(hasMessage(compare(original, s), "Added item: Main Camera"), "added occurrence recorded");
	s = original;
	s.scenes.erase(s.scenes.begin());
	check(hasMessage(compare(original, s), "Removed scene: Live"), "removed scene recorded");
	s = original;
	s.scenes.push_back({"new-scene", "New scene", {}});
	check(hasMessage(compare(original, s), "Added scene: New scene"), "added scene recorded");
	s = original;
	s.sources.push_back({"new-device", "New device", "camera", false, true, false, false});
	check(hasMessage(compare(original, s), "Added: New device"), "new source recorded");
	s = original;
	s.globals[0].sourceId = "mic-b";
	check(hasMessage(compare(original, s), "Channel 1 changed: Host Microphone"),
	      "global audio reassignment recorded");
	s = original;
	s.globals.erase(s.globals.begin());
	check(hasMessage(compare(original, s), "Channel 1 removed"), "global audio removal recorded");
	s = original;
	s.globals.push_back({3, "mic-a"});
	check(hasMessage(compare(original, s), "Channel 3 added"), "global audio addition recorded");

	/* Multiple appearances in arbitrary order must neither disappear nor be
	 * counted in unrelated scenes. */
	for (int n = 0; n < 60; ++n) {
		Snapshot x;
		x.collection = "stress";
		x.sources.push_back({"a", "A", "video", false, true, false, false});
		x.sources.push_back({"b", "B", "video", false, true, false, false});
		Scene scene{"s", "Scene", {}};
		for (int i = 0; i < n; ++i)
			scene.items.push_back({std::to_string(i), i % 2 ? "a" : "b", "item", i % 3 != 0});
		x.scenes.push_back(scene);
		Simulation result = simulate(x, "a", "b");
		int expected = n / 2;
		int actual = result.scenes.empty()
				     ? 0
				     : result.scenes[0].primary.enabled + result.scenes[0].primary.disabled;
		check(result.ok && actual == expected, "generated occurrence count invariant");
	}

	std::printf("\n%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
