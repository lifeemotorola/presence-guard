/*
Presence Guard for OBS Studio
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>

#include <cstdarg>
#include <cstdio>

/* Replaces the generated plugin-support.c for the test harness. */
const char *PLUGIN_NAME = "presence-guard";
const char *PLUGIN_VERSION = "0.0.0-test";

void obs_log(int log_level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	std::printf("[%d] ", log_level);
	std::vprintf(format, args);
	std::printf("\n");
	va_end(args);
}

const char *obs_module_text(const char *lookup_string)
{
	return lookup_string;
}
