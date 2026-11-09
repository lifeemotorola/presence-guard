/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Tests for baseline serialization and safe, honest report generation.
Licensed under GPL-2.0-or-later; this program comes WITHOUT ANY WARRANTY.
*/
#include "snapshot-json.hpp"
#include "twin-report.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

static int checks = 0, failures = 0;
static void check(bool value, const char *description)
{
	++checks;
	if (!value) {
		++failures;
		std::printf("FAIL  %s\n", description);
	} else
		std::printf("PASS  %s\n", description);
}

static twin::Snapshot sample()
{
	twin::Snapshot s;
	s.collection = "Show <script>alert(1)</script>";
	s.capturedAt = "2026-11-02T10:00:00Z";
	s.currentSceneId = "live";
	s.sources = {
		{"camera", "Camera <img src=https://invalid.test>", "video", false, true, false, false},
		{"mic", "Mïc & voice", "audio", true, false, true, false},
	};
	s.scenes = {{"live", "Live & <Studio>", {{"live/1", "camera", "Camera <img src=https://invalid.test>", true}}}};
	s.globals = {{1, "mic"}};
	return s;
}

static QByteArray mutateRoot(const QByteArray &bytes, const char *key, const QJsonValue &value)
{
	QJsonObject root = QJsonDocument::fromJson(bytes).object();
	root[key] = value;
	return QJsonDocument(root).toJson();
}

int main()
{
	const twin::Snapshot original = sample();
	const QByteArray encoded = snapshotToJson(original);
	twin::Snapshot decoded;
	QString error;
	check(snapshotFromJson(encoded, decoded, error), "baseline JSON roundtrip succeeds");
	check(decoded.collection == original.collection && decoded.sources[1].name == original.sources[1].name,
	      "Unicode and punctuation survive roundtrip");
	check(twin::compare(original, decoded).ok && twin::compare(original, decoded).changes.empty(),
	      "roundtrip preserves compared configuration");
	check(decoded.currentSceneId == "live" && decoded.globals.size() == 1 && decoded.globals[0].channel == 1,
	      "scene and global audio survive roundtrip");
	check(!encoded.contains("stream_key") && !encoded.contains("password"),
	      "JSON schema does not serialize credentials");
	check(!snapshotFromJson("not JSON", decoded, error), "invalid JSON rejected");
	check(!snapshotFromJson(mutateRoot(encoded, "schema", 99), decoded, error), "unknown schema rejected");
	check(!snapshotFromJson(mutateRoot(encoded, "sources", "oops"), decoded, error),
	      "malformed source list rejected");
	check(!snapshotFromJson(mutateRoot(encoded, "complete", false), decoded, error), "partial baseline rejected");
	check(!snapshotFromJson(QByteArray(8 * 1024 * 1024 + 1, 'x'), decoded, error), "oversize baseline rejected");

	QJsonObject badRoot = QJsonDocument::fromJson(encoded).object();
	QJsonArray sources = badRoot["sources"].toArray();
	sources.append(sources.first());
	badRoot["sources"] = sources;
	check(!snapshotFromJson(QJsonDocument(badRoot).toJson(), decoded, error), "duplicate source ID rejected");
	badRoot = QJsonDocument::fromJson(encoded).object();
	QJsonArray globals = badRoot["globals"].toArray();
	QJsonObject global = globals[0].toObject();
	global["channel"] = 1.5;
	globals[0] = global;
	badRoot["globals"] = globals;
	check(!snapshotFromJson(QJsonDocument(badRoot).toJson(), decoded, error), "fractional audio channel rejected");
	global["channel"] = 64;
	globals[0] = global;
	badRoot["globals"] = globals;
	check(!snapshotFromJson(QJsonDocument(badRoot).toJson(), decoded, error),
	      "out-of-range audio channel rejected");

	const twin::Comparison comparison = twin::compare(original, original);
	const twin::Simulation sim = twin::simulate(original, "camera", "mic");
	const QString html = makeTwinReport(original, &comparison, &sim, false);
	check(!html.contains("<script>alert"), "collection name cannot inject script");
	check(!html.contains("<img src=https://invalid.test>"), "source name cannot inject remote image");
	check(html.contains("&lt;script&gt;") && html.contains("&lt;img"), "unsafe markup is escaped");
	check(html.contains("OBS reports its audio muted"), "muted backup is clearly marked");
	check(html.contains("Backup lacks at least one capability"), "non-equivalent backup is clearly marked");
	check(html.contains("not a live test") && html.contains("not proven visibility or audibility"),
	      "report distinguishes model from real-world test");
	check(html.contains("No changes detected in the specific properties checked"),
	      "unchanged baseline is not labeled a safety pass");
	check(html.contains("Privacy:") && html.contains("names themselves may contain private information"),
	      "report warns before names are shared");
	const QString darkHtml = makeTwinReport(original, nullptr, nullptr, true);
	check(darkHtml.contains("#101a2b") && darkHtml.contains("No failure scenario was run"),
	      "dock report has dark background and honest empty state");

	std::printf("\n%d Qt checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
