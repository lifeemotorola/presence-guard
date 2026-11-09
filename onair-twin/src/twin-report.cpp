/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include "twin-report.hpp"

#include <QStringList>

namespace {
QString escaped(const std::string &text)
{
	return QString::fromUtf8(text.data(), static_cast<int>(text.size())).toHtmlEscaped();
}

QString channels(const std::vector<unsigned int> &values)
{
	QStringList list;
	for (unsigned int value : values)
		list << QString::number(value);
	return list.join(", ");
}

QString countLabel(const twin::Counts &counts)
{
	return QString("%1 enabled path(s), %2 disabled path(s)").arg(counts.enabled).arg(counts.disabled);
}
} // namespace

QString makeTwinReport(const twin::Snapshot &snapshot, const twin::Comparison *comparison,
		       const twin::Simulation *simulation, bool dark)
{
	const QString ink = dark ? "#e5edfa" : "#15243a";
	const QString muted = dark ? "#aebdd0" : "#4a5c70";
	const QString accent = dark ? "#72e3c4" : "#087f67";
	const QString warn = dark ? "#ffc676" : "#a45406";
	const QString bg = dark ? "#101a2b" : "#ffffff";
	QString html = QString("<!doctype html><html><head><meta charset=\"utf-8\"><title>OnAirTwin report</title>") +
		       "<style>body{font-family:'Segoe UI',sans-serif;line-height:1.5;margin:24px;} "
		       "h1{font-size:25px;margin-bottom:4px} h2{font-size:17px;margin-top:24px} "
		       "li{margin-bottom:7px} .muted{opacity:.8} @media print{body{margin:12mm}}</style></head>" +
		       QString("<body style=\"color:%1;background:%2\">").arg(ink, bg);
	html += QString("<p style=\"color:%1\"><b>ONAIRTWIN · REHEARSAL REPORT</b></p>").arg(accent);
	html += "<h1>Configuration rehearsal</h1>";
	html += QString("<p style=\"color:%1\">Read-only, local what-if analysis — not a live test.</p>").arg(muted);
	html += "<p><b>Scene collection:</b> " + escaped(snapshot.collection) + "<br>";
	html += "<b>Scanned (UTC):</b> " + escaped(snapshot.capturedAt) + "<br>";
	html += QString("<b>Inventory:</b> %1 scene(s), %2 source(s), %3 global audio channel(s)</p>")
			.arg(snapshot.scenes.size())
			.arg(snapshot.sources.size())
			.arg(snapshot.globals.size());

	if (!snapshot.complete) {
		html += QString("<p style=\"color:%1\"><b>INCOMPLETE SCAN — do not rely on this model.</b></p>")
				.arg(warn);
		for (const std::string &notice : snapshot.notices)
			html += "<p>" + escaped(notice) + "</p>";
	}

	html += "<h2>Baseline comparison</h2>";
	if (!comparison) {
		html += "<p>No baseline comparison was available. Save a baseline while the show is configured as intended.</p>";
	} else if (!comparison->ok) {
		html += QString("<p style=\"color:%1\"><b>Comparison unavailable:</b> ").arg(warn) +
			escaped(comparison->error) + "</p>";
	} else if (comparison->changes.empty()) {
		html += "<p>No changes detected in the specific properties checked. This is not a health or safety certification.</p>";
	} else {
		html += QString("<p><b>%1 configuration change(s)</b></p><ul>").arg(comparison->changes.size());
		constexpr size_t MAX_SHOWN = 500;
		size_t shown = 0;
		for (const twin::Change &change : comparison->changes) {
			if (shown++ >= MAX_SHOWN)
				break;
			html += "<li><b>" + escaped(change.scope) + ":</b> " + escaped(change.message) + "</li>";
		}
		html += "</ul>";
		if (comparison->changes.size() > MAX_SHOWN)
			html += "<p>Only the first 500 changes are listed. Review the remaining configuration manually.</p>";
	}

	html += "<h2>What-if source failure</h2>";
	if (!simulation) {
		html += "<p>No failure scenario was run. Choose a source and press <b>Simulate failure</b>.</p>";
	} else if (!simulation->ok) {
		html += QString("<p style=\"color:%1\"><b>Simulation unavailable:</b> ").arg(warn) +
			escaped(simulation->error) + "</p>";
	} else {
		html += "<p><b>Assumed unavailable:</b> " + escaped(simulation->primary.name) + "</p>";
		if (simulation->hasBackup) {
			html += "<p><b>Operator-selected backup:</b> " + escaped(simulation->backup.name) + ". ";
			if (simulation->backup.audio && simulation->backup.muted)
				html += QString("<span style=\"color:%1\"><b>OBS reports its audio muted.</b></span> ")
						.arg(warn);
			if ((simulation->primary.audio && !simulation->backup.audio) ||
			    (simulation->primary.video && !simulation->backup.video))
				html += QString("<span style=\"color:%1\"><b>Backup lacks at least one capability of the selected source.</b></span> ")
						.arg(warn);
			html += "Backup operation and switching are not tested or automatic.</p>";
		} else {
			html += "<p>Backup not selected.</p>";
		}
		if (simulation->scenes.empty() && simulation->primaryGlobalChannels.empty())
			html += "<p>No matching scene item or global audio channel was found in this snapshot.</p>";
		if (!simulation->scenes.empty()) {
			html += "<p><b>Scenes referencing the selected source:</b></p><ul>";
			for (const twin::SceneImpact &scene : simulation->scenes) {
				html += "<li><b>" + escaped(scene.sceneName) + "</b>";
				if (scene.current)
					html += " (OBS-reported current scene at scan)";
				html += "<br>Selected: " + countLabel(scene.primary);
				if (simulation->hasBackup) {
					if (scene.backup.enabled || scene.backup.disabled)
						html += "<br>Backup: " + countLabel(scene.backup);
					else
						html += "<br>Backup: not configured in this scene";
				}
				html += "</li>";
			}
			html += "</ul>";
		}
		if (!simulation->primaryGlobalChannels.empty())
			html += "<p><b>Selected source also configured on global audio channel(s):</b> " +
				channels(simulation->primaryGlobalChannels) + "</p>";
		if (!simulation->backupGlobalChannels.empty())
			html += "<p><b>Backup also configured on global audio channel(s):</b> " +
				channels(simulation->backupGlobalChannels) + "</p>";
	}

	html += "<h2>Important limits</h2><p>Enabled and disabled paths describe OBS scene-item visibility settings, "
		"not proven visibility or audibility. This model does not disconnect devices or test actual camera frames, "
		"microphone signal, output-track routing, filters, transforms, transitions, occlusion, network delivery, "
		"or what viewers see. Test the real broadcast separately.</p>";
	html += QString("<p style=\"color:%1\">Privacy: this report includes your scene and source names. It does not intentionally capture OBS source settings, stream keys, URLs, media paths, audio or video; names themselves may contain private information. Review before sharing.</p>")
			.arg(muted);
	html += "<p>OnAirTwin · by Emmanuel Suah</p></body></html>";
	return html;
}
