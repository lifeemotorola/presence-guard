/*
DockBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include "workspace.hpp"

#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMainWindow>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace dockbloom {
namespace {
void fail(QString *error, const QString &message)
{
	if (error)
		*error = message;
}

class UpdatesGuard {
public:
	explicit UpdatesGuard(QMainWindow *window) : window_(window), wasEnabled_(window->updatesEnabled())
	{
		window->setUpdatesEnabled(false);
	}
	~UpdatesGuard()
	{
		if (window_)
			window_->setUpdatesEnabled(wasEnabled_);
	}

private:
	QPointer<QMainWindow> window_;
	bool wasEnabled_;
};
} // namespace

WebPanel::WebPanel(HostBridge *host, const QString &url, QWidget *parent) : QWidget(parent), host_(host), url_(url)
{
	setObjectName("dockbloomWebPanel");
	setAttribute(Qt::WA_NativeWindow);
	layout_ = new QVBoxLayout(this);
	layout_->setContentsMargins(0, 0, 0, 0);
	status_ = new QLabel("Waiting for the OBS browser component…", this);
	status_->setObjectName("webStatus");
	status_->setWordWrap(true);
	status_->setAlignment(Qt::AlignCenter);
	layout_->addWidget(status_);
}

void WebPanel::activate()
{
	if (shuttingDown_ || browser_ || !host_)
		return;
	if (!host_->browserAvailable()) {
		status_->setText(
			"The OBS browser component is unavailable. Install or enable OBS Browser and restart OBS.");
		return;
	}
	QWidget *browser = host_->createBrowser(this, url_);
	if (!browser) {
		status_->setText("The OBS browser could not create this page. Restart OBS and try again.");
		return;
	}
	browser_ = browser;
	status_->hide();
	layout_->addWidget(browser);
	browser->show();
}

void WebPanel::shutdown()
{
	if (shuttingDown_)
		return;
	shuttingDown_ = true;
	QPointer<QWidget> browser = browser_;
	browser_ = nullptr;
	if (browser && host_)
		host_->closeBrowser(browser.data());
	if (browser)
		delete browser.data();
}

bool WebPanel::event(QEvent *event)
{
	/* OBS's OBSDock sends this event when a user closes a dock. Its own
	 * BrowserDock closes CEF at that point; do the same for our web panel. */
	if (event->type() == static_cast<QEvent::Type>(QEvent::User + QEvent::Close) && browser_ && host_) {
		QPointer<WebPanel> guard(this);
		host_->closeBrowser(browser_.data());
		if (!guard)
			return true;
	}
	return QWidget::event(event);
}

Workspace::Workspace(QMainWindow *main, HostBridge *host, const QString &configPath, QObject *parent)
	: QObject(parent),
	  main_(main),
	  host_(host),
	  configPath_(configPath),
	  backupPath_(QFileInfo(configPath).dir().filePath("dock-layout-before-dockbloom.bin"))
{
}

Workspace::~Workspace()
{
	shutdown();
}

bool Workspace::load(QString *error)
{
	writable_ = loadConfig(configPath_, &config_, error);
	return writable_;
}

bool Workspace::registerWeb(const WebDock &web)
{
	if (!main_ || !host_ || webPanels_.contains(web.id))
		return false;
	/* OBS's registration check does not include every custom browser dock.
	 * Never take ownership of a pre-existing dock with the same object name. */
	if (main_->findChild<QDockWidget *>(web.id, Qt::FindDirectChildrenOnly))
		return false;
	auto *panel = new WebPanel(host_, web.url);
	if (!host_->addDock(web.id, web.title, panel)) {
		delete panel;
		return false;
	}
	webPanels_.insert(web.id, panel);
	if (ready_)
		panel->activate();
	return true;
}

void Workspace::registerSavedWebDocks()
{
	if (!writable_ || stopping_)
		return;
	for (const WebDock &web : config_.webDocks)
		registerWeb(web);
}

void Workspace::frontendReady()
{
	if (stopping_ || !host_)
		return;
	ready_ = true;
	host_->initializeBrowser();
	const auto panels = webPanels_;
	for (const QPointer<WebPanel> &panel : panels)
		if (panel)
			panel->activate();
}

void Workspace::shutdown()
{
	if (stopping_)
		return;
	stopping_ = true;
	/* CEF must be closed while obs-browser and the frontend still exist. */
	const auto panels = webPanels_;
	webPanels_.clear();
	for (auto it = panels.cbegin(); it != panels.cend(); ++it) {
		QPointer<WebPanel> panel = it.value();
		QPointer<QDockWidget> dock = panel ? qobject_cast<QDockWidget *>(panel->parentWidget()) : nullptr;
		if (panel)
			panel->shutdown();
		if (host_ && dock && panel && dock->widget() == panel)
			host_->removeDock(it.key());
	}
	if (host_)
		host_->shutdownBrowser();
}

QList<DockInfo> Workspace::availableDocks() const
{
	QList<DockInfo> result;
	if (!main_)
		return result;
	const auto docks = main_->findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
	QMap<QString, int> counts;
	for (QDockWidget *dock : docks) {
		const QString id = dock->objectName();
		if (validDockId(id))
			++counts[id];
	}
	QSet<QString> webIds;
	for (const WebDock &web : config_.webDocks)
		webIds.insert(web.id);
	for (QDockWidget *dock : docks) {
		const QString id = dock->objectName();
		if (!validDockId(id) || counts.value(id) != 1 || id == kControlId || id == kWorkspaceId)
			continue;
		/* If registration failed due to a collision, show the saved web dock as
		 * unavailable, not as somebody else's dock that we can remove. */
		const QPointer<WebPanel> panel = webPanels_.value(id);
		const bool ours = panel && dock->widget() == panel;
		if (webIds.contains(id) && !ours)
			continue;
		const QString title = dock->windowTitle().trimmed().isEmpty() ? id : dock->windowTitle();
		result.append({id, title, dock, ours});
	}
	std::sort(result.begin(), result.end(), [](const DockInfo &left, const DockInfo &right) {
		const int order = QString::compare(left.title, right.title, Qt::CaseInsensitive);
		return order == 0 ? left.id < right.id : order < 0;
	});
	return result;
}

QStringList Workspace::availableMembers() const
{
	QSet<QString> present;
	for (const DockInfo &info : availableDocks())
		present.insert(info.id);
	QStringList result;
	for (const QString &id : config_.members)
		if (present.contains(id))
			result.append(id);
	return result;
}

bool Workspace::save(const Config &next, QString *error)
{
	if (!writable_ || stopping_) {
		fail(error, "Workspace settings are unavailable. The original file was not overwritten.");
		return false;
	}
	if (!saveConfig(configPath_, next, error))
		return false;
	config_ = next;
	return true;
}

bool Workspace::setMember(const QString &id, bool member, QString *error)
{
	if (!validDockId(id) || id == kControlId || id == kWorkspaceId) {
		fail(error, "This dock cannot be added to its own workspace.");
		return false;
	}
	Config next = config_;
	const bool already = next.members.contains(id);
	if (member == already)
		return true;
	if (member) {
		bool available = false;
		for (const DockInfo &dock : availableDocks())
			available = available || dock.id == id;
		if (!available) {
			fail(error, "This dock is no longer available. Refresh the list and try again.");
			return false;
		}
		if (next.members.size() >= kMaxMembers) {
			fail(error, "The workspace supports up to 256 docks.");
			return false;
		}
		next.members.append(id);
	} else {
		next.members.removeAll(id);
	}
	return save(next, error);
}

bool Workspace::addWebDock(const QString &title, const QString &url, QString *createdId, QString *error)
{
	QString normalized;
	if (!validWebTitle(title)) {
		fail(error, "Enter a dock name of 1–100 characters.");
		return false;
	}
	if (!normalizeWebUrl(url, &normalized, error))
		return false;
	if (!ready_ || !browserAvailable()) {
		fail(error, "The OBS browser component is not available. Enable OBS Browser and restart OBS.");
		return false;
	}
	if (config_.webDocks.size() >= kMaxWebDocks || config_.members.size() >= kMaxMembers) {
		fail(error, "The workspace has reached its web or total dock limit.");
		return false;
	}
	const WebDock web{QStringLiteral("dockbloom-web-") + QUuid::createUuid().toString(QUuid::Id128),
			  title.trimmed(), normalized};
	/* Register first, but do not create or load the browser until the settings
	 * are safely committed. Roll back the empty dock if saving fails. */
	const bool wasReady = ready_;
	ready_ = false;
	const bool registered = registerWeb(web);
	ready_ = wasReady;
	if (!registered) {
		fail(error, "OBS could not register the new web dock.");
		return false;
	}
	Config next = config_;
	next.webDocks.append(web);
	next.members.append(web.id);
	if (!save(next, error)) {
		webPanels_.remove(web.id);
		host_->removeDock(web.id);
		return false;
	}
	if (createdId)
		*createdId = web.id;
	if (auto panel = webPanels_.value(web.id))
		panel->activate();
	return true;
}

bool Workspace::removeWebDock(const QString &id, QString *error)
{
	Config next = config_;
	bool found = false;
	for (auto it = next.webDocks.begin(); it != next.webDocks.end(); ++it) {
		if (it->id == id) {
			next.webDocks.erase(it);
			found = true;
			break;
		}
	}
	if (!found) {
		fail(error, "Select a DockBloom web dock to remove.");
		return false;
	}
	next.members.removeAll(id);
	if (!save(next, error))
		return false;
	QPointer<Workspace> guard(this);
	QPointer<WebPanel> panel = webPanels_.take(id);
	QPointer<QDockWidget> dock = panel ? qobject_cast<QDockWidget *>(panel->parentWidget()) : nullptr;
	if (panel)
		panel->shutdown();
	if (!guard)
		return true;
	/* Only unregister a dock that we actually registered and still own. A
	 * failed saved-dock registration may leave the same ID in another plugin. */
	if (host_ && dock && panel && dock->widget() == panel)
		host_->removeDock(id);
	return true;
}

bool Workspace::hasBackup() const
{
	const QFileInfo file(backupPath_);
	return file.isFile() && file.size() > 0;
}

bool Workspace::ensureBackup(QString *error)
{
	if (!main_) {
		fail(error, "The OBS window is no longer available.");
		return false;
	}
	const QFileInfo existing(backupPath_);
	if (existing.exists()) {
		if (existing.isFile() && existing.size() > 0 && existing.size() <= 5 * 1024 * 1024)
			return true;
		fail(error, "The saved layout backup is invalid. It was not overwritten.");
		return false;
	}
	const QByteArray state = main_->saveState();
	if (state.isEmpty()) {
		fail(error, "OBS could not provide a layout backup. Nothing was rearranged.");
		return false;
	}
	QSaveFile file(backupPath_);
	if (!file.open(QIODevice::WriteOnly) || file.write(state) != state.size() || !file.commit()) {
		fail(error, "Could not save the original OBS layout. Nothing was rearranged.");
		return false;
	}
	return true;
}

bool Workspace::apply(const QString &mode, int page, const QString &focusedId, QString *error)
{
	if (!main_ || stopping_ || (mode != "focus" && mode != "grid")) {
		fail(error, "The workspace is unavailable.");
		return false;
	}
	QMap<QString, QPointer<QDockWidget>> byId;
	for (const DockInfo &info : availableDocks())
		byId.insert(info.id, info.dock);
	QList<QString> ids;
	for (const QString &id : config_.members)
		if (byId.value(id))
			ids.append(id);
	if (ids.isEmpty()) {
		fail(error, "Choose at least one available dock first.");
		return false;
	}
	QPointer<QDockWidget> anchor = main_->findChild<QDockWidget *>(kWorkspaceId, Qt::FindDirectChildrenOnly);
	if (!anchor) {
		fail(error, "The DockBloom Workspace dock is unavailable.");
		return false;
	}
	Qt::DockWidgetArea area = main_->dockWidgetArea(anchor);
	if (area == Qt::NoDockWidgetArea)
		area = Qt::RightDockWidgetArea;
	if (!anchor->features().testFlag(QDockWidget::DockWidgetMovable)) {
		fail(error, "Docks are locked in OBS. Unlock Docks before arranging the workspace.");
		return false;
	}
	for (const QString &id : ids) {
		const auto dock = byId.value(id);
		if (dock && (!dock->features().testFlag(QDockWidget::DockWidgetMovable) ||
			     !dock->allowedAreas().testFlag(area))) {
			fail(error, "An included dock is locked or cannot move to the workspace area. Unlock Docks "
				    "or move DockBloom Workspace to a compatible area; nothing was rearranged.");
			return false;
		}
	}
	page = qBound(0, page, (static_cast<int>(ids.size()) - 1) / kGridPageSize);
	const QString focus = ids.contains(focusedId) ? focusedId : ids.first();
	if (!ensureBackup(error))
		return false;
	Config next = config_;
	next.mode = mode;
	next.page = page;
	if (!save(next, error))
		return false;

	QPointer<Workspace> guard(this);
	QPointer<QMainWindow> main = main_;
	UpdatesGuard updates(main.data());
	/* Only native QDockWidgets are moved. Their content stays owned by OBS
	 * or the originating plugin; no live widget is taken or reparented. */
	main->removeDockWidget(anchor);
	if (!guard || !main || !anchor)
		return false;
	/* A floating dock cannot redock until it has a main-window layout entry.
	 * Add it first: setFloating(false) immediately after removeDockWidget()
	 * silently does nothing on Qt for floating docks. */
	main->addDockWidget(area, anchor);
	if (!guard || !main || !anchor)
		return false;
	anchor->setFloating(false);
	if (!guard || !main || !anchor)
		return false;
	if (anchor->isFloating()) {
		fail(error, "The workspace could not be docked. Restore the previous OBS layout if needed.");
		return false;
	}
	anchor->show();
	for (const QString &id : ids) {
		QPointer<QDockWidget> dock = byId.value(id);
		if (!guard || !main || !anchor)
			return false;
		if (!dock)
			continue;
		main->removeDockWidget(dock);
		if (!guard || !main || !anchor)
			return false;
		if (!dock)
			continue;
		main->addDockWidget(area, dock);
		if (!guard || !main || !anchor || !dock)
			return false;
		dock->setFloating(false);
		if (!guard || !main || !anchor || !dock)
			return false;
		if (dock->isFloating()) {
			fail(error, "An included dock could not be docked. Restore the previous OBS layout if needed.");
			return false;
		}
		dock->hide();
	}
	if (!guard || !main || !anchor)
		return false;
	if (mode == "focus") {
		for (const QString &id : ids) {
			if (!guard || !main || !anchor)
				return false;
			if (auto dock = byId.value(id))
				main->tabifyDockWidget(anchor, dock);
		}
		if (!guard || !main || !anchor)
			return false;
		if (auto dock = byId.value(focus)) {
			dock->show();
			if (!guard || !main || !anchor)
				return false;
			if (dock)
				dock->raise();
		}
	} else {
		QList<QPointer<QDockWidget>> visible;
		const int begin = page * kGridPageSize;
		for (int i = begin; i < qMin(begin + kGridPageSize, static_cast<int>(ids.size())); ++i)
			if (auto dock = byId.value(ids[i]))
				visible.append(dock);
		if (visible.size() >= 3 && visible[2])
			main->splitDockWidget(anchor, visible[2], Qt::Vertical);
		if (!guard || !main || !anchor)
			return false;
		if (visible.size() >= 2 && visible[1])
			main->splitDockWidget(anchor, visible[1], Qt::Horizontal);
		if (!guard || !main || !anchor)
			return false;
		if (visible.size() >= 4 && visible[2] && visible[3])
			main->splitDockWidget(visible[2], visible[3], Qt::Horizontal);
		if (!guard || !main || !anchor)
			return false;
		if (!visible.isEmpty() && visible[0])
			main->tabifyDockWidget(anchor, visible[0]);
		for (const auto &dock : visible) {
			if (!guard || !main || !anchor)
				return false;
			if (dock)
				dock->show();
		}
		if (!guard || !main || !anchor)
			return false;
		if (!visible.isEmpty() && visible[0])
			visible[0]->raise();
		/* Splitters can otherwise give a new row only a few pixels. Equalize
		 * the two rows/columns when possible; users can still drag them. */
		if (guard && main && visible.size() >= 3 && visible[0] && visible[2])
			main->resizeDocks({visible[0].data(), visible[2].data()}, {100, 100}, Qt::Vertical);
		if (guard && main && visible.size() >= 2 && visible[0] && visible[1])
			main->resizeDocks({visible[0].data(), visible[1].data()}, {100, 100}, Qt::Horizontal);
		if (guard && main && visible.size() >= 4 && visible[2] && visible[3])
			main->resizeDocks({visible[2].data(), visible[3].data()}, {100, 100}, Qt::Horizontal);
	}
	return guard && main;
}

bool Workspace::restore(QString *error)
{
	if (!main_ || stopping_) {
		fail(error, "The OBS window is no longer available.");
		return false;
	}
	QFile file(backupPath_);
	if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > 5 * 1024 * 1024) {
		fail(error, "No readable DockBloom layout backup is available.");
		return false;
	}
	const QByteArray state = file.readAll();
	file.close();
	QPointer<Workspace> guard(this);
	const bool restored = main_->restoreState(state);
	if (!guard)
		return false;
	if (!restored) {
		fail(error, "OBS could not restore the saved layout. The backup has been kept.");
		return false;
	}
	if (!QFile::remove(backupPath_))
		fail(error, "The layout was restored, but the backup could not be removed.");
	return true;
}
} // namespace dockbloom
