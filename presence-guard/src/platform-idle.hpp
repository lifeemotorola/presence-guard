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

#pragma once

#include <cstdint>

/**
 * Returns the number of milliseconds since the last keyboard/mouse input
 * by the local user.
 *
 * Returns 0 on platforms where idle time cannot be queried, which makes the
 * AFK rule a no-op there instead of firing unexpectedly.
 */
uint64_t pg_idle_ms(void);

/** True when this build can actually measure idle time. */
bool pg_idle_supported(void);
