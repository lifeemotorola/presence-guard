/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Fake libobs/OBS frontend integration tests for the real snapshot adapter.
Licensed under GPL-2.0-or-later; this program comes WITHOUT ANY WARRANTY.
*/
#include "obs-snapshot.hpp"
#include <obs-frontend-api.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

struct obs_sceneitem {
	int64_t id;
	obs_source_t *source;
	bool visible;
	bool group;
	int refs = 0;
};
struct obs_scene {
	std::vector<obs_sceneitem_t *> items;
};
struct obs_source {
	std::string id, name, type;
	uint32_t flags;
	bool muted = false;
	obs_scene_t *scene = nullptr;
	bool group = false;
	int refs = 0;
};
static std::vector<obs_source_t *> frontendScenes;
static obs_source_t *current = nullptr;
static obs_source_t *outputs[MAX_CHANNELS] = {};
static std::string collection = "Rehearsal";
static int acquired = 0;
static int released = 0;
static int itemAcquired = 0;
static int itemReleased = 0;
static std::string fakeConfigRoot;
void fakeSetConfigRoot(const std::string &path)
{
	fakeConfigRoot = path;
}
char *obs_module_config_path(const char *name)
{
	const std::string path = fakeConfigRoot + "/" + name;
	auto *copy = static_cast<char *>(std::malloc(path.size() + 1));
	std::memcpy(copy, path.c_str(), path.size() + 1);
	return copy;
}

const char *obs_source_get_uuid(const obs_source_t *s)
{
	return s->id.c_str();
}
const char *obs_source_get_name(const obs_source_t *s)
{
	return s->name.c_str();
}
const char *obs_source_get_id(const obs_source_t *s)
{
	return s->type.c_str();
}
uint32_t obs_source_get_output_flags(const obs_source_t *s)
{
	return s->flags;
}
bool obs_source_muted(const obs_source_t *s)
{
	return s->muted;
}
void obs_source_release(obs_source_t *s)
{
	if (s) {
		--s->refs;
		++released;
	}
}
obs_scene_t *obs_scene_from_source(const obs_source_t *s)
{
	return s && !s->group ? s->scene : nullptr;
}
obs_scene_t *obs_group_or_scene_from_source(const obs_source_t *s)
{
	return s ? s->scene : nullptr;
}
void obs_sceneitem_addref(obs_sceneitem_t *i)
{
	++i->refs;
	++itemAcquired;
}
void obs_sceneitem_release(obs_sceneitem_t *i)
{
	--i->refs;
	++itemReleased;
}
void obs_scene_enum_items(obs_scene_t *s, bool (*fn)(obs_scene_t *, obs_sceneitem_t *, void *), void *data)
{
	for (auto *i : s->items)
		if (!fn(s, i, data))
			break;
}
obs_source_t *obs_sceneitem_get_source(obs_sceneitem_t *i)
{
	return i->source;
}
bool obs_sceneitem_is_group(obs_sceneitem_t *i)
{
	return i->group;
}
obs_scene_t *obs_sceneitem_group_get_scene(const obs_sceneitem_t *i)
{
	return i->source->scene;
}
int64_t obs_sceneitem_get_id(const obs_sceneitem_t *i)
{
	return i->id;
}
bool obs_sceneitem_visible(obs_sceneitem_t *i)
{
	return i->visible;
}
obs_source_t *obs_get_output_source(uint32_t n)
{
	obs_source_t *s = outputs[n];
	if (s) {
		++s->refs;
		++acquired;
	}
	return s;
}
char *obs_frontend_get_current_scene_collection()
{
	auto *copy = static_cast<char *>(std::malloc(collection.size() + 1));
	std::memcpy(copy, collection.c_str(), collection.size() + 1);
	return copy;
}
obs_source_t *obs_frontend_get_current_scene()
{
	if (current) {
		++current->refs;
		++acquired;
	}
	return current;
}
void obs_frontend_get_scenes(obs_frontend_source_list *list)
{
	list->sources.num = frontendScenes.size();
	list->sources.array = static_cast<obs_source_t **>(std::malloc(frontendScenes.size() * sizeof(obs_source_t *)));
	for (size_t n = 0; n < frontendScenes.size(); ++n) {
		list->sources.array[n] = frontendScenes[n];
		++frontendScenes[n]->refs;
		++acquired;
	}
}
void obs_frontend_source_list_free(obs_frontend_source_list *list)
{
	for (size_t n = 0; n < list->sources.num; ++n)
		obs_source_release(list->sources.array[n]);
	std::free(list->sources.array);
}

static obs_scene uiScene;
static obs_source uiRoot{"ui-live", "Live", "scene", OBS_SOURCE_VIDEO, false, &uiScene};
static obs_source uiCam{"ui-cam", "Main camera", "video", OBS_SOURCE_VIDEO};
static obs_source uiBackup{"ui-backup", "Backup camera", "video", OBS_SOURCE_VIDEO};
static obs_sceneitem uiMainItem{10, &uiCam, true, false};
static obs_sceneitem uiBackupItem{11, &uiBackup, false, false};
void fakeSetupUiFixture()
{
	uiScene.items = {&uiMainItem, &uiBackupItem};
	uiMainItem.visible = true;
	frontendScenes = {&uiRoot};
	current = &uiRoot;
	for (auto &output : outputs)
		output = nullptr;
	collection = "UI fixture";
}
void fakeSetMainVisible(bool visible)
{
	uiMainItem.visible = visible;
}

#ifndef ONAIRTWIN_FAKE_NO_MAIN
static int checks = 0, failures = 0;
static void check(bool value, const char *msg)
{
	++checks;
	if (!value) {
		++failures;
		std::printf("FAIL  %s\n", msg);
	} else
		std::printf("PASS  %s\n", msg);
}

int main()
{
	obs_scene live, nested, group;
	obs_source root{"scene-live", "Live", "scene", OBS_SOURCE_VIDEO, false, &live};
	obs_source child{"nested", "Guest Scene", "scene", OBS_SOURCE_VIDEO, false, &nested};
	obs_source grouped{"group", "Hidden Group", "group", OBS_SOURCE_VIDEO, false, &group, true};
	obs_source cam{"cam", "Cam <A>", "camera", OBS_SOURCE_VIDEO};
	obs_source backup{"backup", "Backup", "camera", OBS_SOURCE_VIDEO};
	obs_source mic{"mic", "Host Mic", "audio", OBS_SOURCE_AUDIO};
	obs_source global{"global", "Global Mic", "audio", OBS_SOURCE_AUDIO};
	obs_sceneitem a{1, &cam, true, false}, b{2, &child, true, false}, g{3, &grouped, false, true};
	obs_sceneitem cm{4, &cam, true, false}, bk{5, &backup, true, false}, gm{6, &mic, true, false};
	live.items = {&a, &b, &g};
	nested.items = {&cm, &bk};
	group.items = {&gm};
	frontendScenes = {&root, &child};
	current = &root;
	outputs[1] = &global;
	outputs[2] = &mic;
	outputs[0] = &cam; // not a global audio source

	const twin::Snapshot s = captureObsSnapshot("2026-11-02T12:00:00Z");
	check(s.complete && twin::validate(s).empty(), "adapter captures a valid scene collection");
	check(s.collection == "Rehearsal" && s.capturedAt == "2026-11-02T12:00:00Z",
	      "collection and timestamp captured");
	check(s.currentSceneId == "scene-live", "current scene captured");
	check(s.scenes.size() == 2 && s.scenes[0].items.size() == 6, "nested scene and group flattened");
	check(s.scenes[0].items[0].enabled && s.scenes[0].items[2].enabled, "nested enabled paths retained");
	check(!s.scenes[0].items[5].enabled, "hidden group disables its descendants in the model");
	check(s.scenes[0].items[5].path == "Hidden Group / Host Mic", "nested source has readable path");
	check(s.globals.size() == 2 && s.globals[0].channel == 1 && s.globals[1].channel == 2,
	      "global audio captured; program channel excluded");
	check(twin::findSource(s, "global") && twin::findSource(s, "global")->audio,
	      "global-only audio source captured");
	twin::Simulation result = twin::simulate(s, "cam", "backup");
	check(result.ok && result.scenes.size() == 2 && result.scenes[0].primary.enabled == 2,
	      "simulation finds duplicated source in nested and root scenes");
	check(result.scenes[0].backup.enabled == 1 && result.scenes[1].backup.enabled == 1,
	      "backup evidence is scene-specific");
	result = twin::simulate(s, "mic", "global");
	check(result.ok && result.primaryGlobalChannels.size() == 1 && result.scenes[0].primary.disabled == 1,
	      "microphone modeled as both global and hidden scene source");
	check(gm.visible && !g.visible && cam.name == "Cam <A>", "scan and simulation leave inputs unchanged");
	check(acquired == released && itemAcquired == itemReleased, "all retained OBS handles were released");

	cam.id.clear();
	twin::Snapshot bad = captureObsSnapshot("now");
	check(!bad.complete && !twin::simulate(bad, "cam", "").ok, "missing UUID prevents a misleading result");
	cam.id = "cam";

	obs_sceneitem cycle{88, &root, true, false};
	nested.items.push_back(&cycle);
	bad = captureObsSnapshot("now");
	check(!bad.complete && !twin::compare(s, bad).ok, "recursive scene loop is bounded and marked incomplete");
	nested.items.pop_back();

	grouped.scene = nullptr;
	bad = captureObsSnapshot("now");
	check(!bad.complete, "uninspectable group is not treated as a leaf device");
	grouped.scene = &group;

	frontendScenes.clear();
	bad = captureObsSnapshot("now");
	check(!bad.complete, "no scenes during load is incomplete");
	check(acquired == released && itemAcquired == itemReleased, "all error paths release their handles");
	std::printf("\n%d adapter checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}

#endif // ONAIRTWIN_FAKE_NO_MAIN
