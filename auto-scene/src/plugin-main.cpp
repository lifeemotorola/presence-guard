/*
Auto Scene Adjust for OBS Studio
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

#include <QMetaObject>
#include <QObject>

#include "scene-adjust.hpp"
#include "scene-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define SCENE_DOCK_ID "auto-scene-dock"

static SceneAdjust *plugin = nullptr;
static SceneDock *dock = nullptr;

static obs_hotkey_id fixHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id undoHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id toggleHotkey = OBS_INVALID_HOTKEY_ID;

/* Hotkeys arrive on whatever thread the input system is using, and moving a
 * scene item is a frontend job. The request is handed back to the plugin's own
 * thread instead of being acted on where it arrives. */
static void runOnPluginThread(void (*action)(SceneAdjust *))
{
	if (!plugin)
		return;

	SceneAdjust *owner = plugin;
	QMetaObject::invokeMethod(owner, [owner, action]() { action(owner); }, Qt::QueuedConnection);
}

static void hotkeyFix(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
		runOnPluginThread([](SceneAdjust *owner) { owner->fixScene(); });
}

static void hotkeyUndo(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
		runOnPluginThread([](SceneAdjust *owner) { owner->undo(); });
}

static void hotkeyToggle(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
		runOnPluginThread([](SceneAdjust *owner) { owner->setAutoRefit(!owner->config.autoRefit); });
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
	plugin = new SceneAdjust();

	dock = new SceneDock(plugin);
	if (!obs_frontend_add_dock_by_id(SCENE_DOCK_ID, obs_module_text("AutoScene.DockTitle"), dock)) {
		obs_log(LOG_WARNING, "failed to register the Auto Scene Adjust dock");
		delete dock;
		dock = nullptr;
	}

	fixHotkey = obs_hotkey_register_frontend("auto_scene_fix", obs_module_text("AutoScene.Hotkey.Fix"), hotkeyFix,
						 nullptr);
	undoHotkey = obs_hotkey_register_frontend("auto_scene_undo", obs_module_text("AutoScene.Hotkey.Undo"),
						  hotkeyUndo, nullptr);
	toggleHotkey = obs_hotkey_register_frontend("auto_scene_toggle", obs_module_text("AutoScene.Hotkey.Toggle"),
						    hotkeyToggle, nullptr);

	obs_frontend_add_event_callback(frontendEvent, nullptr);

	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);

	if (fixHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(fixHotkey);
	if (undoHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(undoHotkey);
	if (toggleHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(toggleHotkey);

	if (plugin)
		plugin->saveNow();

	/* OBS owns the dock widget from here on, so make it inert before the
	 * plugin object goes away. */
	if (dock) {
		dock->detach();
		obs_frontend_remove_dock(SCENE_DOCK_ID);
		dock = nullptr;
	}

	delete plugin;
	plugin = nullptr;

	obs_log(LOG_INFO, "unloaded");
}
