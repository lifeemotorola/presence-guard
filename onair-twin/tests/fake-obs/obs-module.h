#pragma once
#include <cstddef>
#include <cstdint>

#define OBS_SOURCE_AUDIO (1u << 1)
#define OBS_SOURCE_VIDEO (1u << 0)
#define MAX_CHANNELS 64

struct obs_source;
struct obs_scene;
struct obs_sceneitem;
typedef struct obs_source obs_source_t;
typedef struct obs_scene obs_scene_t;
typedef struct obs_sceneitem obs_sceneitem_t;

const char *obs_source_get_uuid(const obs_source_t *);
const char *obs_source_get_name(const obs_source_t *);
const char *obs_source_get_id(const obs_source_t *);
uint32_t obs_source_get_output_flags(const obs_source_t *);
bool obs_source_muted(const obs_source_t *);
void obs_source_release(obs_source_t *);
obs_scene_t *obs_scene_from_source(const obs_source_t *);
obs_scene_t *obs_group_or_scene_from_source(const obs_source_t *);
void obs_sceneitem_addref(obs_sceneitem_t *);
void obs_sceneitem_release(obs_sceneitem_t *);
void obs_scene_enum_items(obs_scene_t *, bool (*)(obs_scene_t *, obs_sceneitem_t *, void *), void *);
obs_source_t *obs_sceneitem_get_source(obs_sceneitem_t *);
bool obs_sceneitem_is_group(obs_sceneitem_t *);
obs_scene_t *obs_sceneitem_group_get_scene(const obs_sceneitem_t *);
int64_t obs_sceneitem_get_id(const obs_sceneitem_t *);
bool obs_sceneitem_visible(obs_sceneitem_t *);
obs_source_t *obs_get_output_source(uint32_t);
char *obs_module_config_path(const char *);
