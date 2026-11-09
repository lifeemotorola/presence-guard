/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <QMetaObject>

#include "twin-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR("Emmanuel Suah")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "OnAirTwin: read-only OBS rehearsal and configuration reports.";
}

#define TWIN_DOCK_ID "onair-twin-dock"
static TwinDock *dock = nullptr;

static void frontendEvent(enum obs_frontend_event event, void *)
{
	if (!dock)
		return;
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		/* Frontend events may arrive off the GUI thread. The receiver is the
		 * dock, so Qt discards the queued call if it is destroyed. */
		QMetaObject::invokeMethod(dock, [target = dock]() { target->invalidate(); }, Qt::QueuedConnection);
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	dock = new TwinDock();
	if (!obs_frontend_add_dock_by_id(TWIN_DOCK_ID, obs_module_text("OnAirTwin.DockTitle"), dock)) {
		obs_log(LOG_WARNING, "failed to register the OnAirTwin dock");
		delete dock;
		dock = nullptr;
		return false;
	}
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	obs_log(LOG_INFO, "loaded (version %s; read-only rehearsal)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	if (dock) {
		obs_frontend_remove_dock(TWIN_DOCK_ID);
		dock = nullptr;
	}
	obs_log(LOG_INFO, "unloaded");
}
