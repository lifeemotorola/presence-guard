/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include "snapshot-json.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <utility>

namespace {
constexpr int SCHEMA = 1;
constexpr qsizetype MAX_FILE_BYTES = 8 * 1024 * 1024;
constexpr qsizetype MAX_STRING_CHARS = 16384;
constexpr qsizetype MAX_ENTRIES = 20000;

QString qstr(const std::string &text)
{
	return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
}

bool getString(const QJsonObject &object, const char *key, std::string &out)
{
	QJsonValue value = object.value(QLatin1String(key));
	if (!value.isString() || value.toString().size() > MAX_STRING_CHARS)
		return false;
	out = value.toString().toUtf8().toStdString();
	return true;
}

bool getBool(const QJsonObject &object, const char *key, bool &out)
{
	QJsonValue value = object.value(QLatin1String(key));
	if (!value.isBool())
		return false;
	out = value.toBool();
	return true;
}

bool getArray(const QJsonObject &object, const char *key, QJsonArray &out)
{
	QJsonValue value = object.value(QLatin1String(key));
	if (!value.isArray())
		return false;
	out = value.toArray();
	return out.size() <= MAX_ENTRIES;
}
} // namespace

QByteArray snapshotToJson(const twin::Snapshot &snapshot)
{
	QJsonObject root;
	root["schema"] = SCHEMA;
	root["collection"] = qstr(snapshot.collection);
	root["capturedAt"] = qstr(snapshot.capturedAt);
	root["currentSceneId"] = qstr(snapshot.currentSceneId);
	root["complete"] = snapshot.complete;

	QJsonArray notices;
	for (const std::string &notice : snapshot.notices)
		notices.append(qstr(notice));
	root["notices"] = notices;

	QJsonArray sources;
	for (const twin::Source &source : snapshot.sources) {
		QJsonObject obj;
		obj["id"] = qstr(source.id);
		obj["name"] = qstr(source.name);
		obj["type"] = qstr(source.type);
		obj["audio"] = source.audio;
		obj["video"] = source.video;
		obj["muted"] = source.muted;
		obj["container"] = source.container;
		sources.append(obj);
	}
	root["sources"] = sources;

	QJsonArray scenes;
	for (const twin::Scene &scene : snapshot.scenes) {
		QJsonObject obj;
		obj["id"] = qstr(scene.id);
		obj["name"] = qstr(scene.name);
		QJsonArray items;
		for (const twin::Occurrence &item : scene.items) {
			QJsonObject itemObj;
			itemObj["key"] = qstr(item.key);
			itemObj["sourceId"] = qstr(item.sourceId);
			itemObj["path"] = qstr(item.path);
			itemObj["enabled"] = item.enabled;
			items.append(itemObj);
		}
		obj["items"] = items;
		scenes.append(obj);
	}
	root["scenes"] = scenes;

	QJsonArray globals;
	for (const twin::GlobalAudio &global : snapshot.globals) {
		QJsonObject obj;
		obj["channel"] = static_cast<int>(global.channel);
		obj["sourceId"] = qstr(global.sourceId);
		globals.append(obj);
	}
	root["globals"] = globals;
	return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool snapshotFromJson(const QByteArray &bytes, twin::Snapshot &out, QString &error)
{
	auto invalid = [&error](const QString &message) {
		error = message;
		return false;
	};
	if (bytes.size() > MAX_FILE_BYTES)
		return invalid("Baseline file is too large.");
	QJsonParseError parseError;
	QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject())
		return invalid("Baseline is not valid JSON.");
	QJsonObject root = document.object();
	if (!root.value("schema").isDouble() || root.value("schema").toInt(-1) != SCHEMA)
		return invalid("Unsupported baseline version.");

	twin::Snapshot value;
	if (!getString(root, "collection", value.collection) || !getString(root, "capturedAt", value.capturedAt) ||
	    !getString(root, "currentSceneId", value.currentSceneId) || !getBool(root, "complete", value.complete))
		return invalid("Baseline header is incomplete.");

	QJsonArray notices, sources, scenes, globals;
	if (!getArray(root, "notices", notices) || !getArray(root, "sources", sources) ||
	    !getArray(root, "scenes", scenes) || !getArray(root, "globals", globals))
		return invalid("Baseline data is incomplete or too large.");
	for (const QJsonValue &notice : notices) {
		if (!notice.isString() || notice.toString().size() > MAX_STRING_CHARS)
			return invalid("Invalid baseline notice.");
		value.notices.push_back(notice.toString().toUtf8().toStdString());
	}
	for (const QJsonValue &entry : sources) {
		if (!entry.isObject())
			return invalid("Invalid baseline source.");
		QJsonObject obj = entry.toObject();
		twin::Source source;
		if (!getString(obj, "id", source.id) || !getString(obj, "name", source.name) ||
		    !getString(obj, "type", source.type) || !getBool(obj, "audio", source.audio) ||
		    !getBool(obj, "video", source.video) || !getBool(obj, "muted", source.muted) ||
		    !getBool(obj, "container", source.container))
			return invalid("Invalid baseline source fields.");
		value.sources.push_back(std::move(source));
	}

	qsizetype totalItems = 0;
	for (const QJsonValue &entry : scenes) {
		if (!entry.isObject())
			return invalid("Invalid baseline scene.");
		QJsonObject obj = entry.toObject();
		twin::Scene scene;
		QJsonArray items;
		if (!getString(obj, "id", scene.id) || !getString(obj, "name", scene.name) ||
		    !getArray(obj, "items", items))
			return invalid("Invalid baseline scene fields.");
		totalItems += items.size();
		if (totalItems > MAX_ENTRIES)
			return invalid("Baseline has too many scene items.");
		for (const QJsonValue &itemEntry : items) {
			if (!itemEntry.isObject())
				return invalid("Invalid baseline scene item.");
			QJsonObject itemObj = itemEntry.toObject();
			twin::Occurrence item;
			if (!getString(itemObj, "key", item.key) || !getString(itemObj, "sourceId", item.sourceId) ||
			    !getString(itemObj, "path", item.path) || !getBool(itemObj, "enabled", item.enabled))
				return invalid("Invalid baseline scene item fields.");
			scene.items.push_back(std::move(item));
		}
		value.scenes.push_back(std::move(scene));
	}

	for (const QJsonValue &entry : globals) {
		if (!entry.isObject())
			return invalid("Invalid baseline global-audio entry.");
		QJsonObject obj = entry.toObject();
		QJsonValue channel = obj.value("channel");
		twin::GlobalAudio global;
		if (!channel.isDouble() || channel.toDouble() != channel.toInt(-1) || channel.toInt(-1) <= 0 ||
		    channel.toInt(-1) >= 64 || !getString(obj, "sourceId", global.sourceId))
			return invalid("Invalid baseline global-audio fields.");
		global.channel = static_cast<unsigned int>(channel.toInt());
		value.globals.push_back(std::move(global));
	}

	std::string validation = twin::validate(value);
	if (!validation.empty())
		return invalid(qstr(validation));
	out = std::move(value);
	error.clear();
	return true;
}
