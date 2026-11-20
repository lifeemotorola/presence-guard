/*
DockBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/bmem.h>

#include "workspace.hpp"
#include "workspace-ui.hpp"
#include "browser-panel.hpp"

#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <memory>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR("Emmanuel Suah")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "DockBloom: an OBS dock workspace for existing and web docks.";
}

namespace {
class ObsBridge final : public dockbloom::HostBridge {
public:
	~ObsBridge() override { shutdownBrowser(); }

	bool addDock(const QString &id, const QString &title, QWidget *content) override
	{
		const QByteArray dockId = id.toUtf8(), dockTitle = title.toUtf8();
		return obs_frontend_add_dock_by_id(dockId.constData(), dockTitle.constData(), content);
	}
	void removeDock(const QString &id) override
	{
		const QByteArray dockId = id.toUtf8();
		obs_frontend_remove_dock(dockId.constData());
	}
	bool initializeBrowser() override
	{
		if (cef_)
			return true;
		/* This interface is pinned to the obs-browser shipped with OBS 33.0.1.
		 * Reject an unknown version rather than calling an incompatible vtable. */
		if (obs_browser_qcef_version() != 3)
			return false;
		cef_ = obs_browser_init_panel();
		return cef_ != nullptr;
	}
	bool browserAvailable() const override { return cef_ != nullptr; }
	QWidget *createBrowser(QWidget *parent, const QString &url) override
	{
		if (!cef_)
			return nullptr;
		QCefWidget *browser = cef_->create_widget(parent, url.toUtf8().toStdString(), nullptr);
		if (browser)
			browser->allowAllPopups(true); // Match OBS custom browser docks.
		return browser;
	}
	void closeBrowser(QWidget *browser) override
	{
		if (cef_ && browser)
			static_cast<QCefWidget *>(browser)->closeBrowser();
	}
	void shutdownBrowser() override
	{
		delete cef_;
		cef_ = nullptr;
	}

private:
	QCef *cef_ = nullptr;
};

std::unique_ptr<ObsBridge> bridge;
QPointer<dockbloom::Workspace> workspace;
QPointer<dockbloom::WorkspaceUi> control;
bool controlRegistered = false;
bool anchorRegistered = false;
bool cleanedUp = false;

void cleanup()
{
	if (cleanedUp)
		return;
	cleanedUp = true;
	if (workspace)
		workspace->shutdown();
	if (bridge) {
		if (anchorRegistered)
			bridge->removeDock(dockbloom::kWorkspaceId);
		if (controlRegistered)
			bridge->removeDock(dockbloom::kControlId);
	}
	anchorRegistered = false;
	controlRegistered = false;
	delete workspace.data();
	workspace = nullptr;
	control = nullptr;
	bridge.reset();
}

void frontendEvent(enum obs_frontend_event event, void *)
{
	if (!workspace)
		return;
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		QMetaObject::invokeMethod(
			workspace.data(),
			[] {
				if (workspace) {
					workspace->frontendReady();
					if (control)
						control->refresh();
				}
			},
			Qt::QueuedConnection);
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		/* OBS 33 emits EXIT on the GUI thread, before destroying the frontend
		 * and browser module. Release CEF widgets while both still exist. */
		if (QThread::currentThread() == workspace->thread())
			cleanup();
		else
			QMetaObject::invokeMethod(workspace.data(), [] { cleanup(); }, Qt::BlockingQueuedConnection);
	}
}
} // namespace

bool obs_module_load(void)
{
	cleanedUp = false;
	auto *main = qobject_cast<QMainWindow *>(static_cast<QWidget *>(obs_frontend_get_main_window()));
	if (!main)
		return false;
	char *path = obs_module_config_path("workspace.json");
	if (!path)
		return false;
	const QString configPath = QString::fromUtf8(path);
	bfree(path);
	bridge = std::make_unique<ObsBridge>();
	workspace = new dockbloom::Workspace(main, bridge.get(), configPath);
	QString error;
	const bool loaded = workspace->load(&error);
	control = new dockbloom::WorkspaceUi(workspace.data());
	if (!bridge->addDock(dockbloom::kControlId, obs_module_text("DockBloom.DockTitle"), control.data())) {
		delete control.data();
		control = nullptr;
		cleanup();
		return false;
	}
	controlRegistered = true;
	QWidget *anchor = dockbloom::createWorkspaceAnchor();
	if (!bridge->addDock(dockbloom::kWorkspaceId, obs_module_text("DockBloom.WorkspaceTitle"), anchor)) {
		delete anchor;
		cleanup();
		return false;
	}
	anchorRegistered = true;
	/* Saved placeholders are registered before OBS restores its dock layout;
	 * the browser itself is attached only after all OBS modules have loaded. */
	if (loaded)
		workspace->registerSavedWebDocks();
	else if (control)
		control->showError(error);
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	obs_log(LOG_INFO, "loaded (version %s; native OBS dock workspace)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	cleanup();
	obs_log(LOG_INFO, "unloaded");
}
