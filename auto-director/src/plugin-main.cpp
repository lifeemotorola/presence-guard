/*
Auto Director for OBS Studio
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
#include <obs-hotkey.h>
#include <plugin-support.h>

#include "auto-director.hpp"
#include "director-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define AD_DOCK_ID "auto-director-dock"

/* Not "director": that is the name of the policy namespace. */
static Director *theDirector = nullptr;
static DirectorDock *dock = nullptr;

static obs_hotkey_id toggleHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id nextHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id holdHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id lobbyHotkey = OBS_INVALID_HOTKEY_ID;

/* Hotkeys can fire from any thread, so they only post a request; the director
 * applies it on the Qt main thread on its next pass. */
static void hotkeyToggle(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed && theDirector)
		theDirector->toggleEnabled();
}

static void hotkeyNext(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed && theDirector)
		theDirector->nextTake();
}

static void hotkeyHold(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed && theDirector)
		theDirector->lockNow();
}

static void hotkeyLobby(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed && theDirector)
		theDirector->fallbackNow();
}

static void frontendEvent(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	if (!theDirector)
		return;

	theDirector->handleEvent(event);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		if (dock)
			dock->reloadLists();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	theDirector = new Director();
	theDirector->loadSettings();

	dock = new DirectorDock(theDirector);
	if (!obs_frontend_add_dock_by_id(AD_DOCK_ID, obs_module_text("AutoDirector.DockTitle"), dock)) {
		obs_log(LOG_WARNING, "failed to register the Auto Director dock");
		delete dock;
		dock = nullptr;
	}

	toggleHotkey = obs_hotkey_register_frontend(
		"auto_director_toggle", obs_module_text("AutoDirector.Hotkey.Toggle"), hotkeyToggle, nullptr);
	nextHotkey = obs_hotkey_register_frontend("auto_director_next", obs_module_text("AutoDirector.Hotkey.Next"),
						  hotkeyNext, nullptr);
	holdHotkey = obs_hotkey_register_frontend("auto_director_hold", obs_module_text("AutoDirector.Hotkey.Hold"),
						  hotkeyHold, nullptr);
	lobbyHotkey = obs_hotkey_register_frontend("auto_director_lobby", obs_module_text("AutoDirector.Hotkey.Lobby"),
						   hotkeyLobby, nullptr);

	obs_frontend_add_event_callback(frontendEvent, nullptr);

	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);

	if (toggleHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(toggleHotkey);
	if (nextHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(nextHotkey);
	if (holdHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(holdHotkey);
	if (lobbyHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(lobbyHotkey);

	if (theDirector)
		theDirector->saveNow();

	/* OBS owns the dock widget from here on, so make it inert before the
	 * director object goes away. */
	if (dock) {
		dock->detach();
		obs_frontend_remove_dock(AD_DOCK_ID);
		dock = nullptr;
	}

	delete theDirector;
	theDirector = nullptr;

	obs_log(LOG_INFO, "unloaded");
}
