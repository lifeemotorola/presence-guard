/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <QMetaObject>

#include "mix-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR("Emmanuel Suah")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "MixBloom: an audio mixer and track-routing dock for existing OBS sources.";
}

#define MIXBLOOM_DOCK_ID "mixbloom-dock"
static MixDock *dock = nullptr;

static void frontendEvent(enum obs_frontend_event event, void *)
{
	if (!dock)
		return;
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
	case OBS_FRONTEND_EVENT_EXIT:
		/* A receiver-bound queued call runs on the GUI thread and is dropped
		 * by Qt if the dock has already been destroyed. */
		QMetaObject::invokeMethod(dock, [target = dock] { target->suspendForCollectionChange(); },
					  Qt::QueuedConnection);
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		QMetaObject::invokeMethod(dock, [target = dock] { target->resumeAfterCollectionChange(); },
					  Qt::QueuedConnection);
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	dock = new MixDock();
	if (!obs_frontend_add_dock_by_id(MIXBLOOM_DOCK_ID, obs_module_text("MixBloom.DockTitle"), dock)) {
		obs_log(LOG_WARNING, "failed to register the MixBloom dock");
		delete dock;
		dock = nullptr;
		return false;
	}
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	obs_log(LOG_INFO, "loaded (version %s; controls existing OBS audio sources)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	if (dock) {
		obs_frontend_remove_dock(MIXBLOOM_DOCK_ID);
		dock = nullptr;
	}
	obs_log(LOG_INFO, "unloaded");
}
