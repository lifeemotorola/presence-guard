#pragma once
#include <cstdint>
#include <obs-module.h>
EXPORT int config_save(config_t *config);
EXPORT void config_set_string(config_t *config, const char *section, const char *name, const char *value);
EXPORT void config_set_int(config_t *config, const char *section, const char *name, int64_t value);
EXPORT void config_set_bool(config_t *config, const char *section, const char *name, bool value);
EXPORT const char *config_get_string(config_t *config, const char *section, const char *name);
EXPORT int64_t config_get_int(config_t *config, const char *section, const char *name);
EXPORT bool config_get_bool(config_t *config, const char *section, const char *name);
