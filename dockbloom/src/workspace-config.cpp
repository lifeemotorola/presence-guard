/*
DockBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include "workspace-config.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>

namespace dockbloom {
namespace {
void fail(QString *error, const QString &message)
{
	if (error)
		*error = message;
}

bool hasControlCharacters(const QString &text)
{
	for (const QChar character : text) {
		if (character.category() == QChar::Other_Control)
			return true;
	}
	return false;
}

bool validate(const Config &config, QString *error)
{
	if (config.members.size() > kMaxMembers || config.webDocks.size() > kMaxWebDocks) {
		fail(error, "The saved workspace exceeds the supported dock limit.");
		return false;
	}
	if ((config.mode != "focus" && config.mode != "grid") || config.page < 0 ||
	    config.page > kMaxMembers / kGridPageSize) {
		fail(error, "The saved workspace view is invalid.");
		return false;
	}
	QSet<QString> memberIds;
	for (const QString &id : config.members) {
		if (!validDockId(id) || memberIds.contains(id)) {
			fail(error, "The saved workspace has an invalid or duplicate dock ID.");
			return false;
		}
		memberIds.insert(id);
	}
	QSet<QString> webIds;
	for (const WebDock &web : config.webDocks) {
		QString normalized;
		if (!validWebDockId(web.id) || webIds.contains(web.id) || !validWebTitle(web.title) ||
		    !normalizeWebUrl(web.url, &normalized, nullptr)) {
			fail(error, "The saved workspace has an invalid or duplicate web dock.");
			return false;
		}
		webIds.insert(web.id);
	}
	return true;
}
} // namespace

bool validDockId(const QString &id)
{
	return !id.isEmpty() && id.size() <= 256 && !hasControlCharacters(id);
}

bool validWebDockId(const QString &id)
{
	static const QRegularExpression pattern(QStringLiteral("^dockbloom-web-[0-9a-f]{32}$"));
	return pattern.match(id).hasMatch();
}

bool validWebTitle(const QString &title)
{
	return !title.trimmed().isEmpty() && title.size() <= 100 && !hasControlCharacters(title);
}

bool normalizeWebUrl(const QString &input, QString *normalized, QString *error)
{
	const QString trimmed = input.trimmed();
	if (trimmed.isEmpty() || trimmed.size() > 4096 || hasControlCharacters(trimmed)) {
		fail(error, "Enter a valid web address of at most 4096 characters.");
		return false;
	}
	const QUrl url(trimmed, QUrl::StrictMode);
	const QString scheme = url.scheme().toLower();
	if (!url.isValid() || (scheme != "http" && scheme != "https") || url.host().isEmpty()) {
		fail(error, "Use a complete http:// or https:// address with a host.");
		return false;
	}
	if (!url.userInfo().isEmpty()) {
		fail(error, "Addresses containing a username or password are not supported.");
		return false;
	}
	if (normalized)
		*normalized = url.toString(QUrl::FullyEncoded);
	return true;
}

bool loadConfig(const QString &path, Config *config, QString *error)
{
	if (!config) {
		fail(error, "No destination was provided for the workspace.");
		return false;
	}
	if (!QFileInfo::exists(path)) {
		*config = Config{};
		return true;
	}
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
		fail(error, "Could not read the workspace file, or it is larger than 1 MiB.");
		return false;
	}
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		fail(error, "The workspace file is not valid JSON. It has not been overwritten.");
		return false;
	}
	const QJsonObject root = doc.object();
	if (root.value("version").toInt(-1) != 1 || !root.value("members").isArray() ||
	    !root.value("webDocks").isArray() || !root.value("mode").isString() || !root.value("page").isDouble()) {
		fail(error, "The workspace file has an unsupported or incomplete format. It has not been overwritten.");
		return false;
	}
	Config loaded;
	loaded.mode = root.value("mode").toString();
	loaded.page = root.value("page").toInt(-1);
	for (const QJsonValue &value : root.value("members").toArray()) {
		if (!value.isString()) {
			fail(error, "A saved dock ID is not text. The file has not been overwritten.");
			return false;
		}
		loaded.members.append(value.toString());
	}
	for (const QJsonValue &value : root.value("webDocks").toArray()) {
		if (!value.isObject()) {
			fail(error, "A saved web dock is invalid. The file has not been overwritten.");
			return false;
		}
		const QJsonObject web = value.toObject();
		if (!web.value("id").isString() || !web.value("title").isString() || !web.value("url").isString()) {
			fail(error, "A saved web dock is incomplete. The file has not been overwritten.");
			return false;
		}
		loaded.webDocks.append(
			{web.value("id").toString(), web.value("title").toString(), web.value("url").toString()});
	}
	if (!validate(loaded, error))
		return false;
	*config = loaded;
	return true;
}

bool saveConfig(const QString &path, const Config &config, QString *error)
{
	if (!validate(config, error))
		return false;
	if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
		fail(error, "Could not create the workspace settings folder.");
		return false;
	}
	QJsonArray members, webDocks;
	for (const QString &id : config.members)
		members.append(id);
	for (const WebDock &web : config.webDocks)
		webDocks.append(QJsonObject{{"id", web.id}, {"title", web.title}, {"url", web.url}});
	const QJsonObject root{{"version", 1},
			       {"members", members},
			       {"webDocks", webDocks},
			       {"mode", config.mode},
			       {"page", config.page}};
	const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
		fail(error, "Could not safely save the workspace. No layout change was applied.");
		return false;
	}
	return true;
}
} // namespace dockbloom
