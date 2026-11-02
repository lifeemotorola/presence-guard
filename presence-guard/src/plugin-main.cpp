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

#include "presence-guard.hpp"
#include "presence-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define PG_DOCK_ID "presence-guard-dock"

static PresenceGuard *presenceGuard = nullptr;
static PresenceDock *presenceDock = nullptr;

static void frontendEvent(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	if (!presenceGuard)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
		/* The scene collection is restored after modules load, so the
		 * configured microphone and scenes usually only exist now. */
		config_t *config = obs_frontend_get_profile_config();
		if (config)
			presenceGuard->loadFromConfig(config);

		presenceGuard->restoreNow();
		presenceGuard->refreshVolmeter();

		if (presenceDock)
			presenceDock->refreshFromGuard();
		break;
	}
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		presenceGuard->restoreNow();
		presenceGuard->refreshVolmeter();

		if (presenceDock)
			presenceDock->refreshFromGuard();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		/* Carry the current settings over into the new profile. */
		presenceGuard->saveNow();
		presenceGuard->refreshVolmeter();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		presenceGuard->restoreNow();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		presenceGuard->saveNow();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	presenceGuard = new PresenceGuard();

	config_t *config = obs_frontend_get_profile_config();
	if (config)
		presenceGuard->loadFromConfig(config);

	presenceDock = new PresenceDock(presenceGuard);

	if (!obs_frontend_add_dock_by_id(PG_DOCK_ID, obs_module_text("PresenceGuard.DockTitle"), presenceDock)) {
		obs_log(LOG_WARNING, "failed to register the Presence Guard dock");
		delete presenceDock;
		presenceDock = nullptr;
	}

	obs_frontend_add_event_callback(frontendEvent, nullptr);

	presenceGuard->refreshVolmeter();

	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);

	if (presenceGuard)
		presenceGuard->saveNow();

	/* OBS owns the dock widget from here on, so make it inert before the
	 * guard object goes away. */
	if (presenceDock) {
		presenceDock->detach();
		obs_frontend_remove_dock(PG_DOCK_ID);
		presenceDock = nullptr;
	}

	delete presenceGuard;
	presenceGuard = nullptr;

	obs_log(LOG_INFO, "unloaded");
}
