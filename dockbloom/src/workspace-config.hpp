/*
DockBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
GPL-2.0-or-later; see LICENSE. No warranty.
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace dockbloom {
constexpr int kMaxMembers = 256;
constexpr int kMaxWebDocks = 100;
constexpr int kGridPageSize = 4;

struct WebDock {
	QString id;
	QString title;
	QString url;
};

struct Config {
	QStringList members;
	QList<WebDock> webDocks;
	QString mode = "focus";
	int page = 0;
};

bool validDockId(const QString &id);
bool validWebDockId(const QString &id);
bool normalizeWebUrl(const QString &input, QString *normalized, QString *error);
bool validWebTitle(const QString &title);
bool loadConfig(const QString &path, Config *config, QString *error);
bool saveConfig(const QString &path, const Config &config, QString *error);
} // namespace dockbloom
