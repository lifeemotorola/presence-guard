/* Test-only OBS meter declarations. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
#pragma once
#include "obs-module.h"

enum obs_fader_type { OBS_FADER_CUBIC };
struct obs_volmeter;
typedef obs_volmeter obs_volmeter_t;
typedef void (*obs_volmeter_updated_t)(void *, const float[MAX_AUDIO_CHANNELS], const float[MAX_AUDIO_CHANNELS],
				       const float[MAX_AUDIO_CHANNELS]);
obs_volmeter_t *obs_volmeter_create(enum obs_fader_type);
void obs_volmeter_destroy(obs_volmeter_t *);
bool obs_volmeter_attach_source(obs_volmeter_t *, obs_source_t *);
void obs_volmeter_add_callback(obs_volmeter_t *, obs_volmeter_updated_t, void *);
void obs_volmeter_remove_callback(obs_volmeter_t *, obs_volmeter_updated_t, void *);
