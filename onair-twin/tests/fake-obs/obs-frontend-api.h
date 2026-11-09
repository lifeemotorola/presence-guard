#pragma once
#include "obs-module.h"
struct obs_frontend_source_list {
	struct {
		obs_source_t **array;
		size_t num;
	} sources;
};
char *obs_frontend_get_current_scene_collection();
obs_source_t *obs_frontend_get_current_scene();
void obs_frontend_get_scenes(obs_frontend_source_list *);
void obs_frontend_source_list_free(obs_frontend_source_list *);
