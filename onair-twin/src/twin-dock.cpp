/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#include "twin-dock.hpp"
#include "obs-snapshot.hpp"
#include "snapshot-json.hpp"
#include "twin-report.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
constexpr qint64 MAX_BASELINE_BYTES = 8 * 1024 * 1024;

QString qstr(const std::string &value)
{
	return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

std::string utf8(const QString &value)
{
	return value.toUtf8().toStdString();
}

QLabel *label(const QString &text, const char *name, QWidget *parent)
{
	auto *value = new QLabel(text, parent);
	value->setObjectName(name);
	value->setWordWrap(true);
	value->setTextFormat(Qt::PlainText);
	return value;
}

QFrame *card(QWidget *parent, QVBoxLayout **layout)
{
	auto *frame = new QFrame(parent);
	frame->setObjectName("twinCard");
	*layout = new QVBoxLayout(frame);
	(*layout)->setContentsMargins(16, 15, 16, 15);
	(*layout)->setSpacing(8);
	return frame;
}

QPushButton *button(const QString &text, const char *name, QWidget *parent)
{
	auto *value = new QPushButton(text, parent);
	value->setObjectName(name);
	value->setMinimumHeight(36);
	value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	return value;
}
} // namespace

TwinDock::TwinDock(QWidget *parent) : QWidget(parent)
{
	buildUi();
	invalidate();
}

void TwinDock::buildUi()
{
	setObjectName("onairTwinRoot");
	setStyleSheet(R"(
		QWidget#onairTwinRoot { background: #0a101d; }
		QWidget { color: #e5eefb; font-family: 'Segoe UI'; font-size: 13px; }
		QScrollArea { border: none; background: transparent; }
		QWidget#scrollBody { background: #0a101d; }
		QFrame#twinHero { background: #12283a; border: 1px solid #29495b; border-radius: 12px; }
		QFrame#twinCard { background: #121d2e; border: 1px solid #26384d; border-radius: 12px; }
		QLabel#eyebrow { color: #78e5c6; font-size: 10px; font-weight: 700; }
		QLabel#brand { color: #f4f8ff; font-size: 25px; font-weight: 700; }
		QLabel#subtitle { color: #b9cbe0; font-size: 12px; }
		QLabel#cardTitle { color: #f4f8ff; font-size: 16px; font-weight: 700; }
		QLabel#caption { color: #afc0d4; font-size: 12px; }
		QLabel#status { color: #ffc984; background: #263246; border: 1px solid #42546a;
		                border-radius: 8px; padding: 9px; }
		QLabel#field { color: #c9d9e9; font-size: 12px; font-weight: 600; }
		QPushButton { border: 1px solid #40546c; border-radius: 7px; padding: 7px 10px;
		              background: #203149; color: #eaf3ff; font-weight: 600; }
		QPushButton:hover { background: #2a405d; border-color: #66809e; }
		QPushButton:disabled { color: #6d8197; background: #182436; border-color: #293a4e; }
		QPushButton#primaryButton { background: #71dfc1; color: #092721; border: 1px solid #71dfc1; }
		QPushButton#primaryButton:hover { background: #9bf0d9; }
		QPushButton#primaryButton:disabled { background: #28483f; color: #729a8f; border-color: #35594d; }
		QComboBox { background: #0c1727; border: 1px solid #40546c; border-radius: 7px; padding: 8px; }
		QComboBox QAbstractItemView { background: #142337; color: #eaf3ff; selection-background-color: #315372; }
		QTextBrowser { background: #101a2b; color: #e5edfa; border: 1px solid #26384d; border-radius: 9px; padding: 6px; }
		QScrollBar:vertical { background: #0a101d; width: 10px; margin: 1px; }
		QScrollBar::handle:vertical { background: #465c72; min-height: 20px; border-radius: 4px; }
		QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
	)");

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	auto *body = new QWidget(scroll);
	body->setObjectName("scrollBody");
	auto *content = new QVBoxLayout(body);
	content->setContentsMargins(12, 12, 12, 12);
	content->setSpacing(10);

	auto *hero = new QFrame(body);
	hero->setObjectName("twinHero");
	auto *heroLayout = new QVBoxLayout(hero);
	heroLayout->setContentsMargins(17, 16, 17, 16);
	heroLayout->setSpacing(4);
	heroLayout->addWidget(label("PRODUCTION ASSURANCE  /  BETA", "eyebrow", hero));
	heroLayout->addWidget(label("OnAirTwin", "brand", hero));
	heroLayout->addWidget(label("Rehearse a failure. Protect the show.", "subtitle", hero));
	heroLayout->addWidget(label("READ-ONLY  ·  LOCAL  ·  NO ACCOUNT", "eyebrow", hero));
	content->addWidget(hero);

	status_ = label("", "status", body);
	content->addWidget(status_);

	QVBoxLayout *scanLayout = nullptr;
	QFrame *scanCard = card(body, &scanLayout);
	scanLayout->addWidget(label("01  /  Scan your setup", "cardTitle", scanCard));
	scanLayout->addWidget(label(
		"Map scenes, nested groups and global audio. Save a trusted baseline to detect later configuration changes.",
		"caption", scanCard));
	auto *scanButton = button("Scan OBS setup", "primaryButton", scanCard);
	saveButton_ = button("Save current baseline", "secondaryButton", scanCard);
	scanLayout->addWidget(scanButton);
	scanLayout->addWidget(saveButton_);
	content->addWidget(scanCard);

	QVBoxLayout *simLayout = nullptr;
	QFrame *simCard = card(body, &simLayout);
	simLayout->addWidget(label("02  /  Rehearse a failure", "cardTitle", simCard));
	simLayout->addWidget(label(
		"Choose a source and an optional backup. This only analyzes a fresh copy of the configuration; it never changes OBS.",
		"caption", simCard));
	simLayout->addWidget(label("ASSUME THIS SOURCE FAILS", "field", simCard));
	primary_ = new QComboBox(simCard);
	primary_->setAccessibleName("Source to simulate as unavailable");
	primary_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	primary_->setMinimumContentsLength(8);
	primary_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	simLayout->addWidget(primary_);
	simLayout->addWidget(label("OPERATOR-SELECTED BACKUP  /  OPTIONAL", "field", simCard));
	backup_ = new QComboBox(simCard);
	backup_->setAccessibleName("Optional backup source");
	backup_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	backup_->setMinimumContentsLength(8);
	backup_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	simLayout->addWidget(backup_);
	simulateButton_ = button("Simulate failure", "primaryButton", simCard);
	simLayout->addWidget(simulateButton_);
	content->addWidget(simCard);

	QVBoxLayout *reportLayout = nullptr;
	QFrame *reportCard = card(body, &reportLayout);
	reportLayout->addWidget(label("03  /  Review the evidence", "cardTitle", reportCard));
	reportLayout->addWidget(label(
		"Configuration evidence, not a camera, audio or audience playback test. Review source names before sharing.",
		"caption", reportCard));
	report_ = new QTextBrowser(reportCard);
	report_->setAccessibleName("OnAirTwin rehearsal report");
	report_->setOpenLinks(false);
	report_->setOpenExternalLinks(false);
	report_->setMinimumHeight(300);
	report_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	reportLayout->addWidget(report_);
	exportButton_ = button("Export HTML report", "secondaryButton", reportCard);
	reportLayout->addWidget(exportButton_);
	content->addWidget(reportCard);
	content->addWidget(label("By Emmanuel Suah  ·  OnAirTwin 0.1 beta", "caption", body));
	content->addStretch();
	scroll->setWidget(body);
	outer->addWidget(scroll);

	connect(scanButton, &QPushButton::clicked, this, [this]() { scan(true); });
	connect(saveButton_, &QPushButton::clicked, this, [this]() { saveBaseline(); });
	connect(simulateButton_, &QPushButton::clicked, this, [this]() { simulateFailure(); });
	connect(exportButton_, &QPushButton::clicked, this, [this]() { exportReport(); });
	connect(primary_, &QComboBox::currentIndexChanged, this, [this]() { clearSimulation(); });
	connect(backup_, &QComboBox::currentIndexChanged, this, [this]() { clearSimulation(); });
}

QString TwinDock::selectedId(const QComboBox *combo) const
{
	return combo ? combo->currentData().toString() : QString();
}

void TwinDock::invalidate()
{
	haveSnapshot_ = false;
	simulation_.reset();
	comparison_.reset();
	current_ = twin::Snapshot{};
	status_->setText("Ready to scan. Results are a point-in-time model, not live monitoring.");
	QSignalBlocker blockPrimary(primary_);
	QSignalBlocker blockBackup(backup_);
	primary_->clear();
	backup_->clear();
	saveButton_->setEnabled(false);
	simulateButton_->setEnabled(false);
	exportButton_->setEnabled(false);
	report_->setHtml("<p>Scan your OBS setup to start a local rehearsal.</p>");
}

void TwinDock::populateSources(const QString &primaryId, const QString &backupId)
{
	QSignalBlocker blockPrimary(primary_);
	QSignalBlocker blockBackup(backup_);
	primary_->clear();
	backup_->clear();
	backup_->addItem("No backup selected", QString());
	for (const twin::Source &source : current_.sources) {
		if (source.container)
			continue;
		QString category;
		if (source.audio && source.video)
			category = "Audio + video";
		else if (source.audio)
			category = "Audio";
		else if (source.video)
			category = "Video";
		else
			category = "Other";
		const QString name = qstr(source.name) + "  ·  " + category;
		const QString id = qstr(source.id);
		primary_->addItem(name, id);
		primary_->setItemData(primary_->count() - 1, name, Qt::ToolTipRole);
		backup_->addItem(name, id);
		backup_->setItemData(backup_->count() - 1, name, Qt::ToolTipRole);
	}
	int primaryIndex = primary_->findData(primaryId);
	if (primaryIndex >= 0)
		primary_->setCurrentIndex(primaryIndex);
	int backupIndex = backup_->findData(backupId);
	if (backupIndex >= 0)
		backup_->setCurrentIndex(backupIndex);
}

bool TwinDock::scan(bool keepSelection)
{
	const QString oldPrimary = keepSelection ? selectedId(primary_) : QString();
	const QString oldBackup = keepSelection ? selectedId(backup_) : QString();
	updating_ = true;
	current_ = captureObsSnapshot(utf8(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)));
	haveSnapshot_ = true;
	simulation_.reset();
	comparison_.reset();
	populateSources(oldPrimary, oldBackup);
	const std::string error = twin::validate(current_);
	const bool valid = error.empty();
	if (valid) {
		loadBaseline();
		status_->setText(
			QString("Scan captured %1 scene(s) and %2 source(s). Results describe settings at the scan time.")
				.arg(current_.scenes.size())
				.arg(current_.sources.size()));
	} else {
		status_->setText("Scan incomplete: " + qstr(error));
	}
	saveButton_->setEnabled(valid);
	simulateButton_->setEnabled(valid && primary_->count() > 0);
	exportButton_->setEnabled(valid);
	updating_ = false;
	refreshReport();
	return valid;
}

QString TwinDock::baselinePath() const
{
	char *path = obs_module_config_path("baseline-v1.json");
	const QString result = path ? QString::fromUtf8(path) : QString();
	bfree(path);
	return result;
}

void TwinDock::loadBaseline()
{
	const QString path = baselinePath();
	if (path.isEmpty()) {
		twin::Comparison c;
		c.error = "OBS could not locate the plugin configuration folder.";
		comparison_ = c;
		return;
	}
	QFile file(path);
	if (!file.exists())
		return;
	twin::Comparison c;
	if (!file.open(QIODevice::ReadOnly) || file.size() > MAX_BASELINE_BYTES) {
		c.error = "The saved baseline could not be read or is too large.";
		comparison_ = c;
		return;
	}
	twin::Snapshot baseline;
	QString error;
	if (!snapshotFromJson(file.readAll(), baseline, error)) {
		c.error = utf8("The saved baseline is invalid: " + error);
		comparison_ = c;
		return;
	}
	comparison_ = twin::compare(baseline, current_);
}

void TwinDock::saveBaseline()
{
	if (!scan(true))
		return;
	const QString path = baselinePath();
	if (path.isEmpty()) {
		QMessageBox::warning(this, "OnAirTwin", "OBS could not locate the plugin configuration folder.");
		return;
	}
	if (QFileInfo::exists(path) &&
	    QMessageBox::question(this, "Replace baseline?",
				  "Replace the existing baseline with this collection's current configuration? "
				  "The previous baseline will no longer be available.") != QMessageBox::Yes)
		return;

	QByteArray data = snapshotToJson(current_);
	twin::Snapshot check;
	QString error;
	if (!snapshotFromJson(data, check, error)) {
		QMessageBox::warning(this, "Baseline not saved", error);
		return;
	}
	if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
		QMessageBox::warning(this, "Baseline not saved", "Could not create the plugin configuration folder.");
		return;
	}
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
		QMessageBox::warning(this, "Baseline not saved",
				     "Could not write the baseline. Check folder permissions and free space.");
		return;
	}
	comparison_ = twin::compare(check, current_);
	status_->setText(
		"Baseline saved locally. Rescan before each show to compare the properties this plugin checks.");
	refreshReport();
}

void TwinDock::simulateFailure()
{
	const QString oldPrimary = selectedId(primary_);
	const QString oldBackup = selectedId(backup_);
	if (oldPrimary.isEmpty())
		return;
	if (!scan(true))
		return;
	if (primary_->findData(oldPrimary) < 0 || (!oldBackup.isEmpty() && backup_->findData(oldBackup) < 0)) {
		status_->setText("The selected source changed or disappeared during the fresh scan. Choose again.");
		return;
	}
	simulation_ = twin::simulate(current_, utf8(oldPrimary), utf8(oldBackup));
	if (simulation_->ok)
		status_->setText("What-if complete. No device, scene, mute setting or broadcast was changed.");
	else
		status_->setText("Simulation unavailable: " + qstr(simulation_->error));
	refreshReport();
}

void TwinDock::clearSimulation()
{
	if (updating_ || !simulation_)
		return;
	simulation_.reset();
	status_->setText("Selection changed. Run a fresh simulation before relying on the report.");
	refreshReport();
}

void TwinDock::refreshReport()
{
	if (!haveSnapshot_)
		return;
	report_->setHtml(makeTwinReport(current_, comparison_ ? &*comparison_ : nullptr,
					simulation_ ? &*simulation_ : nullptr, true));
}

void TwinDock::exportReport()
{
	if (!haveSnapshot_ || !twin::validate(current_).empty())
		return;
	const QString path = QFileDialog::getSaveFileName(
		this, "Save OnAirTwin report", QDir::homePath() + "/OnAirTwin-report.html", "HTML report (*.html)");
	if (path.isEmpty())
		return;
	const QByteArray bytes = makeTwinReport(current_, comparison_ ? &*comparison_ : nullptr,
						simulation_ ? &*simulation_ : nullptr, false)
					 .toUtf8();
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
		QMessageBox::warning(this, "Report not saved",
				     "Could not write the report. Check folder permissions and free space.");
		return;
	}
	status_->setText("Report saved. It includes scene and source names; review it before sharing.");
}
