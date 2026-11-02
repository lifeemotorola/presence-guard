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

#include "platform-idle.hpp"

/* Test double for src/platform-idle.cpp: the harness drives idle time
 * directly instead of asking the operating system. */
uint64_t g_testIdleMs = 0;

uint64_t pg_idle_ms(void)
{
	return g_testIdleMs;
}

bool pg_idle_supported(void)
{
	return true;
}
