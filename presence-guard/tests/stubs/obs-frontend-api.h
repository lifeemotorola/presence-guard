#pragma once
#include <cstddef>
#include <obs-module.h>
#include <util/config-file.h>

struct obs_frontend_source_list;

enum obs_frontend_event {
	OBS_FRONTEND_EVENT_STREAMING_STARTED,
	OBS_FRONTEND_EVENT_STREAMING_STOPPED,
	OBS_FRONTEND_EVENT_RECORDING_STARTED,
	OBS_FRONTEND_EVENT_RECORDING_STOPPED,
	OBS_FRONTEND_EVENT_SCENE_CHANGED,
	OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED,
	OBS_FRONTEND_EVENT_PROFILE_CHANGED,
	OBS_FRONTEND_EVENT_EXIT,
	OBS_FRONTEND_EVENT_FINISHED_LOADING,
};

template <typename T> struct darray_t { T *array; size_t num; size_t capacity; };

struct obs_frontend_source_list { darray_t<obs_source_t *> sources; };

static inline void obs_frontend_source_list_free(struct obs_frontend_source_list *source_list)
{
	for (size_t i = 0; i < source_list->sources.num; i++)
		obs_source_release(source_list->sources.array[i]);
	source_list->sources.array = nullptr;
	source_list->sources.num = 0;
}

EXPORT void obs_frontend_get_scenes(struct obs_frontend_source_list *sources);
EXPORT obs_source_t *obs_frontend_get_current_scene(void);
EXPORT void obs_frontend_set_current_scene(obs_source_t *scene);
EXPORT bool obs_frontend_add_dock_by_id(const char *id, const char *title, void *widget);
EXPORT void obs_frontend_remove_dock(const char *id);
typedef void (*obs_frontend_event_cb)(enum obs_frontend_event event, void *private_data);
EXPORT void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *private_data);
EXPORT void obs_frontend_remove_event_callback(obs_frontend_event_cb callback, void *private_data);
EXPORT bool obs_frontend_streaming_active(void);
EXPORT bool obs_frontend_recording_active(void);
EXPORT config_t *obs_frontend_get_profile_config(void);
EXPORT bool obs_frontend_preview_program_mode_active(void);
