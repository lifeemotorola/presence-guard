/*
Hype Clip for OBS Studio
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

#include "hype-clip.hpp"
#include "hype-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define HYPE_DOCK_ID "hype-clip-dock"

static HypeClip *plugin = nullptr;
static HypeDock *dock = nullptr;

static obs_hotkey_id clipHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id toggleHotkey = OBS_INVALID_HOTKEY_ID;

/* Hotkeys can fire from any thread. clipNow() only talks to the frontend, so
 * the request is handed to the dock's thread through the plugin's own queue in
 * the poll timer instead of being acted on here. */
static void hotkeyClip(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed && plugin)
		plugin->clipNow("manual");
}

static void hotkeyToggle(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed && plugin)
		plugin->toggleAutoClip();
}

static void frontendEvent(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	if (!plugin)
		return;

	plugin->handleEvent(event);

	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && dock)
		dock->syncFromPlugin();
}

bool obs_module_load(void)
{
	plugin = new HypeClip();
	plugin->loadSettings();

	dock = new HypeDock(plugin);
	if (!obs_frontend_add_dock_by_id(HYPE_DOCK_ID, obs_module_text("HypeClip.DockTitle"), dock)) {
		obs_log(LOG_WARNING, "failed to register the Hype Clip dock");
		delete dock;
		dock = nullptr;
	}

	clipHotkey = obs_hotkey_register_frontend("hype_clip_clip", obs_module_text("HypeClip.Hotkey.Clip"), hotkeyClip,
						  nullptr);
	toggleHotkey = obs_hotkey_register_frontend("hype_clip_toggle", obs_module_text("HypeClip.Hotkey.Toggle"),
						    hotkeyToggle, nullptr);

	obs_frontend_add_event_callback(frontendEvent, nullptr);

	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);

	if (clipHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(clipHotkey);
	if (toggleHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(toggleHotkey);

	if (plugin)
		plugin->saveNow();

	/* OBS owns the dock widget from here on, so make it inert before the
	 * plugin object goes away. */
	if (dock) {
		dock->detach();
		obs_frontend_remove_dock(HYPE_DOCK_ID);
		dock = nullptr;
	}

	delete plugin;
	plugin = nullptr;

	obs_log(LOG_INFO, "unloaded");
}
