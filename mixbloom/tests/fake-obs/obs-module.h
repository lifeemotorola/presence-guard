/* Test-only OBS declarations. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
#pragma once
#include <cstdint>

#define OBS_SOURCE_AUDIO (1u << 1)
#define OBS_SOURCE_VIDEO (1u << 0)
#define OBS_SOURCE_DO_NOT_SELF_MONITOR (1u << 9)
#define OBS_SOURCE_SUBMIX (1u << 12)
#define MAX_AUDIO_CHANNELS 8

struct obs_source;
struct obs_weak_source;
struct obs_data;
typedef obs_source obs_source_t;
typedef obs_weak_source obs_weak_source_t;
typedef obs_data obs_data_t;

void obs_enum_sources(bool (*callback)(void *, obs_source_t *), void *);
obs_source_t *obs_get_source_by_uuid(const char *);
void obs_source_release(obs_source_t *);
obs_weak_source_t *obs_source_get_weak_source(obs_source_t *);
obs_source_t *obs_weak_source_get_source(obs_weak_source_t *);
void obs_weak_source_release(obs_weak_source_t *);
bool obs_source_removed(const obs_source_t *);
uint32_t obs_source_get_output_flags(const obs_source_t *);
const char *obs_source_get_uuid(const obs_source_t *);
const char *obs_source_get_name(const obs_source_t *);
float obs_source_get_volume(const obs_source_t *);
void obs_source_set_volume(obs_source_t *, float);
bool obs_source_muted(const obs_source_t *);
void obs_source_set_muted(obs_source_t *, bool);
uint32_t obs_source_get_audio_mixers(const obs_source_t *);
void obs_source_set_audio_mixers(obs_source_t *, uint32_t);
bool obs_source_get_monitoring_enabled(const obs_source_t *);
void obs_source_set_monitoring_enabled(obs_source_t *, bool);
obs_data_t *obs_source_get_private_settings(obs_source_t *);
bool obs_data_get_bool(const obs_data_t *, const char *);
void obs_data_release(obs_data_t *);
