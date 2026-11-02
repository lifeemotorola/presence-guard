#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

#define EXPORT
#define UNUSED_PARAMETER(param) (void)param

enum { LOG_ERROR = 100, LOG_WARNING = 200, LOG_INFO = 300, LOG_DEBUG = 400 };

#ifdef __cplusplus
#define MODULE_EXPORT extern "C"
#define MODULE_EXTERN extern "C"
#else
#define MODULE_EXPORT
#define MODULE_EXTERN extern
#endif

struct obs_source; typedef struct obs_source obs_source_t;
struct obs_scene; typedef struct obs_scene obs_scene_t;
struct obs_volmeter; typedef struct obs_volmeter obs_volmeter_t;
struct obs_module; typedef struct obs_module obs_module_t;
struct config_data; typedef struct config_data config_t;
struct obs_data; typedef struct obs_data obs_data_t;

#define MAX_AUDIO_CHANNELS 8
#define OBS_SOURCE_AUDIO (1u << 3)

EXPORT obs_source_t *obs_get_source_by_name(const char *name);
EXPORT void obs_source_release(obs_source_t *source);
EXPORT const char *obs_source_get_name(const obs_source_t *source);
EXPORT uint32_t obs_source_get_output_flags(const obs_source_t *source);
EXPORT obs_scene_t *obs_scene_from_source(const obs_source_t *source);
EXPORT void obs_enum_sources(bool (*enum_proc)(void *, obs_source_t *), void *param);

enum obs_fader_type { OBS_FADER_CUBIC, OBS_FADER_IEC, OBS_FADER_LOG };
enum obs_peak_meter_type { SAMPLE_PEAK_METER, TRUE_PEAK_METER };
typedef void (*obs_volmeter_updated_t)(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
                                       const float peak[MAX_AUDIO_CHANNELS],
                                       const float input_peak[MAX_AUDIO_CHANNELS]);
EXPORT obs_volmeter_t *obs_volmeter_create(enum obs_fader_type type);
EXPORT void obs_volmeter_destroy(obs_volmeter_t *volmeter);
EXPORT bool obs_volmeter_attach_source(obs_volmeter_t *volmeter, obs_source_t *source);
EXPORT void obs_volmeter_detach_source(obs_volmeter_t *volmeter);
EXPORT void obs_volmeter_set_peak_meter_type(obs_volmeter_t *volmeter, enum obs_peak_meter_type type);
EXPORT void obs_volmeter_add_callback(obs_volmeter_t *volmeter, obs_volmeter_updated_t cb, void *param);
EXPORT void obs_volmeter_remove_callback(obs_volmeter_t *volmeter, obs_volmeter_updated_t cb, void *param);

EXPORT const char *obs_module_text(const char *lookup_string);
EXPORT obs_module_t *obs_current_module(void);

MODULE_EXPORT bool obs_module_load(void);
MODULE_EXPORT void obs_module_unload(void);
MODULE_EXPORT void obs_module_set_locale(const char *locale);
MODULE_EXPORT void obs_module_free_locale(void);

#define OBS_DECLARE_MODULE()                                              \
	static obs_module_t *obs_module_pointer;                          \
	MODULE_EXPORT void obs_module_set_pointer(obs_module_t *module);  \
	void obs_module_set_pointer(obs_module_t *module)                 \
	{                                                                 \
		obs_module_pointer = module;                              \
	}                                                                 \
	obs_module_t *obs_current_module(void)                            \
	{                                                                 \
		return obs_module_pointer;                                \
	}                                                                 \
	MODULE_EXPORT uint32_t obs_module_ver(void);                      \
	uint32_t obs_module_ver(void)                                     \
	{                                                                 \
		return 0;                                                 \
	}

#define OBS_MODULE_USE_DEFAULT_LOCALE(module_name, default_locale)                        \
	const char *obs_module_text(const char *val)                                      \
	{                                                                                 \
		return val;                                                               \
	}                                                                                 \
	void obs_module_set_locale(const char *locale)                                     \
	{                                                                                 \
		(void)locale;                                                             \
	}                                                                                 \
	void obs_module_free_locale(void)                                                  \
	{                                                                                 \
	}
