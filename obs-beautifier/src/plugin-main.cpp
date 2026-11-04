/*
OBS Beautifier
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
with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <obs-module.h>
#include <plugin-support.h>

#include "beautifier.hpp"
#include "beautifier-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define BEAUTIFIER_DOCK_ID "obs-beautifier-dock"

static Beautifier *beautifier = nullptr;
static BeautifierDock *dock = nullptr;

static void frontendEvent(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	if (!beautifier)
		return;

	beautifier->handleEvent(event);

	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && dock)
		dock->syncFromBeautifier();
}

bool obs_module_load(void)
{
	beautifier = new Beautifier();
	beautifier->loadSettings();

	dock = new BeautifierDock(beautifier);
	if (!obs_frontend_add_dock_by_id(BEAUTIFIER_DOCK_ID, obs_module_text("Beautifier.DockTitle"), dock)) {
		obs_log(LOG_WARNING, "failed to register the OBS Beautifier dock");
		delete dock;
		dock = nullptr;
	}

	obs_frontend_add_event_callback(frontendEvent, nullptr);

	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);

	/* Take our stylesheet off before the objects that describe it go
	 * away: OBS has to keep running without us. */
	if (beautifier) {
		beautifier->saveNow();
		beautifier->revert();
	}

	/* OBS owns the dock widget from here on, so make it inert before the
	 * beautifier object goes away. */
	if (dock) {
		dock->detach();
		obs_frontend_remove_dock(BEAUTIFIER_DOCK_ID);
		dock = nullptr;
	}

	delete beautifier;
	beautifier = nullptr;

	obs_log(LOG_INFO, "unloaded");
}
