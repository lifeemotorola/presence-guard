/*
DockBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
GPL-2.0-or-later; see LICENSE. No warranty.
*/
#pragma once

#include "workspace-config.hpp"

#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

class QDockWidget;
class QLabel;
class QMainWindow;
class QShowEvent;
class QVBoxLayout;

namespace dockbloom {
constexpr const char *kControlId = "dockbloom-control";
constexpr const char *kWorkspaceId = "dockbloom-workspace";

/* The OBS implementation owns the browser bridge and the frontend dock API.
 * Tests substitute a real QMainWindow and a fake browser, not a second Qt. */
class HostBridge {
public:
	virtual ~HostBridge() = default;
	virtual bool addDock(const QString &id, const QString &title, QWidget *content) = 0;
	virtual void removeDock(const QString &id) = 0;
	virtual bool initializeBrowser() = 0;
	virtual bool browserAvailable() const = 0;
	virtual QWidget *createBrowser(QWidget *parent, const QString &url) = 0;
	virtual void closeBrowser(QWidget *browser) = 0;
	virtual void shutdownBrowser() = 0;
};

class WebPanel final : public QWidget {
public:
	WebPanel(HostBridge *host, const QString &url, QWidget *parent = nullptr);
	void activate();
	void shutdown();
	bool hasBrowser() const { return !browser_.isNull(); }

protected:
	bool event(QEvent *event) override;

private:
	HostBridge *host_;
	QString url_;
	QVBoxLayout *layout_ = nullptr;
	QLabel *status_ = nullptr;
	QPointer<QWidget> browser_;
	bool shuttingDown_ = false;
};

struct DockInfo {
	QString id;
	QString title;
	QPointer<QDockWidget> dock;
	bool web = false;
};

class Workspace final : public QObject {
public:
	Workspace(QMainWindow *main, HostBridge *host, const QString &configPath, QObject *parent = nullptr);
	~Workspace() override;

	bool load(QString *error);
	void registerSavedWebDocks();
	void frontendReady();
	void shutdown();
	bool isWritable() const { return writable_; }
	bool browserAvailable() const { return host_ && host_->browserAvailable(); }
	const Config &config() const { return config_; }
	QString configPath() const { return configPath_; }
	QString backupPath() const { return backupPath_; }
	bool hasBackup() const;
	QList<DockInfo> availableDocks() const;
	QStringList availableMembers() const;
	bool setMember(const QString &id, bool member, QString *error);
	bool addWebDock(const QString &title, const QString &url, QString *createdId, QString *error);
	bool removeWebDock(const QString &id, QString *error);
	bool apply(const QString &mode, int page, const QString &focusedId, QString *error);
	bool restore(QString *error);

private:
	bool registerWeb(const WebDock &web);
	bool save(const Config &next, QString *error);
	bool ensureBackup(QString *error);
	QPointer<QMainWindow> main_;
	HostBridge *host_;
	QString configPath_;
	QString backupPath_;
	Config config_;
	QMap<QString, QPointer<WebPanel>> webPanels_;
	bool writable_ = false;
	bool ready_ = false;
	bool stopping_ = false;
};
} // namespace dockbloom
