/*
Hype Clip for OBS Studio
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "hype-dock.hpp"
#include "hype-clip.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

/* The meter is drawn the way OBS's own audio meters are: -60 dB to 0 dB. */
static const int METER_LOW_DB = -60;
static const int METER_HIGH_DB = 0;

QString HypeDock::trText(const char *key) const
{
	return QString::fromUtf8(obs_module_text(key));
}

HypeDock::HypeDock(HypeClip *owner, QWidget *parent) : QWidget(parent), owner_(owner)
{
	buildUi();

	statusTimer_ = new QTimer(this);
	statusTimer_->setInterval(250);
	QObject::connect(statusTimer_, &QTimer::timeout, [this]() {
		refreshStatus();
		refreshClips();
	});
	statusTimer_->start();

	syncFromPlugin();
	refreshStatus();
	refreshClips();
}

/* ------------------------------------------------------------------------- */
/* construction                                                              */
/* ------------------------------------------------------------------------- */

void HypeDock::buildUi()
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(6);

	clipButton_ = new QPushButton(trText("HypeClip.ClipThat"), this);
	clipButton_->setToolTip(trText("HypeClip.ClipThat.Tooltip"));
	QFont clipFont = clipButton_->font();
	clipFont.setBold(true);
	clipFont.setPointSize(clipFont.pointSize() + 2);
	clipButton_->setFont(clipFont);
	clipButton_->setMinimumHeight(38);
	QObject::connect(clipButton_, &QPushButton::clicked, [this]() {
		if (owner_)
			owner_->clipNow("manual");
		refreshStatus();
	});
	layout->addWidget(clipButton_);

	autoClipBox_ = new QCheckBox(trText("HypeClip.AutoClip"), this);
	autoClipBox_->setToolTip(trText("HypeClip.AutoClip.Tooltip"));
	QObject::connect(autoClipBox_, &QCheckBox::toggled, [this](bool) { pushConfig(); });
	layout->addWidget(autoClipBox_);

	QGroupBox *statusGroup = new QGroupBox(trText("HypeClip.Status"), this);
	QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
	statusLabel_ = new QLabel(statusGroup);
	QFont bold = statusLabel_->font();
	bold.setBold(true);
	statusLabel_->setFont(bold);
	detailLabel_ = new QLabel(statusGroup);
	detailLabel_->setWordWrap(true);

	meterBar_ = new QProgressBar(statusGroup);
	meterBar_->setRange(METER_LOW_DB, METER_HIGH_DB);
	meterBar_->setValue(METER_LOW_DB);
	meterBar_->setTextVisible(true);
	meterBar_->setFormat(QStringLiteral("%v dB"));

	statusLayout->addWidget(statusLabel_);
	statusLayout->addWidget(meterBar_);
	statusLayout->addWidget(detailLabel_);
	layout->addWidget(statusGroup);

	QGroupBox *rulesGroup = new QGroupBox(trText("HypeClip.Rules"), this);
	QGridLayout *grid = new QGridLayout(rulesGroup);
	grid->setColumnStretch(1, 1);

	int row = 0;
	auto addSpin = [&](const char *labelText, const char *suffix, int low, int high, int step, QSpinBox **out) {
		QLabel *label = new QLabel(trText(labelText), rulesGroup);
		QSpinBox *spin = new QSpinBox(rulesGroup);
		spin->setRange(low, high);
		spin->setSingleStep(step);
		spin->setSuffix(QString::fromUtf8(suffix));
		QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int) { pushConfig(); });
		grid->addWidget(label, row, 0);
		grid->addWidget(spin, row, 1);
		row++;
		*out = spin;
	};

	addSpin("HypeClip.Threshold", " dB", -60, 0, 1, &thresholdSpin_);
	addSpin("HypeClip.Margin", " dB", 0, 30, 1, &marginSpin_);
	addSpin("HypeClip.Sustain", " ms", 100, 5000, 100, &sustainSpin_);
	addSpin("HypeClip.Cooldown", " s", 0, 600, 5, &cooldownSpin_);

	layout->addWidget(rulesGroup);

	QGroupBox *clipsGroup = new QGroupBox(trText("HypeClip.Clips"), this);
	QVBoxLayout *clipsLayout = new QVBoxLayout(clipsGroup);

	clipTable_ = new QTableWidget(0, 3, clipsGroup);
	clipTable_->setHorizontalHeaderLabels(QStringList()
					      << trText("HypeClip.Column.When") << trText("HypeClip.Column.Score")
					      << trText("HypeClip.Column.Why"));
	clipTable_->verticalHeader()->setVisible(false);
	clipTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
	clipTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	clipTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	clipTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	clipTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	clipTable_->setMinimumHeight(110);
	QObject::connect(clipTable_, &QTableWidget::cellDoubleClicked, [this](int, int) {
		const QString path = selectedClipPath();
		if (path.isEmpty())
			return;
		QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
	});
	clipsLayout->addWidget(clipTable_);

	QHBoxLayout *clipButtons = new QHBoxLayout();
	folderButton_ = new QPushButton(trText("HypeClip.OpenFolder"), clipsGroup);
	copyButton_ = new QPushButton(trText("HypeClip.CopyPath"), clipsGroup);
	deleteButton_ = new QPushButton(trText("HypeClip.DeleteFile"), clipsGroup);
	clearButton_ = new QPushButton(trText("HypeClip.ClearList"), clipsGroup);
	clipButtons->addWidget(folderButton_);
	clipButtons->addWidget(copyButton_);
	clipButtons->addWidget(deleteButton_);
	clipButtons->addWidget(clearButton_);
	clipsLayout->addLayout(clipButtons);

	layout->addWidget(clipsGroup);

	QObject::connect(folderButton_, &QPushButton::clicked, [this]() {
		if (!owner_)
			return;

		const std::string folder = owner_->clipsFolder();
		if (folder.empty())
			return;

		QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(folder)));
	});

	QObject::connect(copyButton_, &QPushButton::clicked, [this]() {
		const QString path = selectedClipPath();
		if (path.isEmpty())
			return;

		QApplication::clipboard()->setText(path);
	});

	QObject::connect(deleteButton_, &QPushButton::clicked, [this]() {
		if (!owner_)
			return;

		const int row = clipTable_->currentRow();
		if (row < 0)
			return;

		owner_->deleteClipFile((size_t)row);
		refreshClips();
	});

	QObject::connect(clearButton_, &QPushButton::clicked, [this]() {
		if (!owner_)
			return;

		owner_->clearClips();
		refreshClips();
	});

	QLabel *hint = new QLabel(trText("HypeClip.Hint"), this);
	hint->setWordWrap(true);
	hint->setEnabled(false);
	layout->addWidget(hint);

	layout->addStretch(1);
}

/* ------------------------------------------------------------------------- */
/* state                                                                     */
/* ------------------------------------------------------------------------- */

void HypeDock::syncFromPlugin()
{
	if (!owner_ || loading_)
		return;

	loading_ = true;

	const hype::Config &config = owner_->config;

	autoClipBox_->setChecked(config.autoClip);
	thresholdSpin_->setValue(config.thresholdDb);
	marginSpin_->setValue(config.marginDb);
	sustainSpin_->setValue(config.sustainMs);
	cooldownSpin_->setValue(config.cooldownMs / 1000);

	loading_ = false;
}

void HypeDock::pushConfig()
{
	if (!owner_ || loading_)
		return;

	hype::Config config = owner_->config;

	config.enabled = true;
	config.autoClip = autoClipBox_->isChecked();
	config.thresholdDb = thresholdSpin_->value();
	config.marginDb = marginSpin_->value();
	config.sustainMs = sustainSpin_->value();
	config.cooldownMs = cooldownSpin_->value() * 1000;

	owner_->applyConfig(config);
}

void HypeDock::refreshStatus()
{
	if (!owner_)
		return;

	statusLabel_->setText(owner_->statusText());
	detailLabel_->setText(owner_->detailText());

	const hype::Status status = owner_->status();
	meterBar_->setValue((int)(status.db < METER_LOW_DB ? METER_LOW_DB : status.db));
}

QString HypeDock::selectedClipPath() const
{
	const int row = clipTable_->currentRow();
	if (row < 0)
		return QString();

	QTableWidgetItem *item = clipTable_->item(row, 0);
	if (!item)
		return QString();

	return item->data(Qt::UserRole).toString();
}

void HypeDock::refreshClips()
{
	if (!owner_)
		return;

	const std::vector<ClipEntry> &clips = owner_->clips();

	int64_t stamp = -1;
	if (!clips.empty())
		stamp = clips.front().savedMs;

	if (stamp == lastClipStamp_ && (int)clips.size() == lastClipCount_)
		return;

	lastClipStamp_ = stamp;
	lastClipCount_ = (int)clips.size();

	clipTable_->setRowCount((int)clips.size());

	for (int row = 0; row < (int)clips.size(); row++) {
		const ClipEntry &entry = clips[(size_t)row];

		const QString when = QDateTime::fromMSecsSinceEpoch((qint64)entry.savedMs).toString("HH:mm:ss");
		const QString score = entry.scoreDb <= -99.0f ? QStringLiteral("-")
							      : QStringLiteral("%1 dB").arg((int)entry.scoreDb);
		const QString why = QString::fromStdString(entry.reason);

		QTableWidgetItem *whenItem = new QTableWidgetItem(when);
		whenItem->setData(Qt::UserRole, QString::fromStdString(entry.path));
		whenItem->setToolTip(QString::fromStdString(entry.path));

		clipTable_->setItem(row, 0, whenItem);
		clipTable_->setItem(row, 1, new QTableWidgetItem(score));
		clipTable_->setItem(row, 2, new QTableWidgetItem(why));
	}
}

void HypeDock::detach()
{
	owner_ = nullptr;

	if (statusTimer_)
		statusTimer_->stop();
}
