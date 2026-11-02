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

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

uint64_t pg_idle_ms(void)
{
	LASTINPUTINFO lii;
	lii.cbSize = sizeof(lii);
	lii.dwTime = 0;
	lii.dwTime = 0;

	if (!GetLastInputInfo(&lii))
		return 0;

	/* Both values are 32-bit tick counts and wrap roughly every 49 days;
	 * unsigned arithmetic makes the wrap harmless. */
	DWORD now = GetTickCount();
	DWORD last = (DWORD)lii.dwTime;

	return (uint64_t)(DWORD)(now - last);
}

bool pg_idle_supported(void)
{
	return true;
}

#elif defined(__APPLE__)

#include <CoreGraphics/CoreGraphics.h>

uint64_t pg_idle_ms(void)
{
	CFTimeInterval seconds =
		CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);

	if (seconds <= 0.0)
		return 0;

	return (uint64_t)(seconds * 1000.0);
}

bool pg_idle_supported(void)
{
	return true;
}

#else

/*
 * Linux/X11 would need libX11 + libXss (XScreenSaverQueryInfo) and Wayland has
 * no portable equivalent. Rather than pull extra dependencies into the build,
 * the AFK rule simply stays inactive on this platform.
 */
uint64_t pg_idle_ms(void)
{
	return 0;
}

bool pg_idle_supported(void)
{
	return false;
}

#endif
