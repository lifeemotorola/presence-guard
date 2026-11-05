/*
Auto Scene Adjust for OBS Studio
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

#include "scene-dock.hpp"

#include "scene-adjust.hpp"
#include "scene-core.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <string>

/* The dock follows the plugin, which does its own work twice as often. */
static const int REFRESH_INTERVAL_MS = 400;

/* Column order of the table. */
enum Column { ColumnSource = 0, ColumnSize, ColumnScreen, ColumnState, ColumnMode, ColumnCount };

/* The order of the mode drop down. Anything unrecognised reads as "off". */
static autoscene::FitMode modeForIndex(int index)
{
	switch (index) {
	case 1:
		return autoscene::FitMode::Fit;
	case 2:
		return autoscene::FitMode::Fill;
	case 3:
		return autoscene::FitMode::Center;
	case 4:
		return autoscene::FitMode::Stretch;
	default:
		return autoscene::FitMode::None;
	}
}

static int indexForMode(autoscene::FitMode mode)
{
	switch (mode) {
	case autoscene::FitMode::Fit:
		return 1;
	case autoscene::FitMode::Fill:
		return 2;
	case autoscene::FitMode::Center:
		return 3;
	case autoscene::FitMode::Stretch:
		return 4;
	case autoscene::FitMode::None:
		break;
	}
	return 0;
}

SceneDock::SceneDock(SceneAdjust *owner, QWidget *parent) : QWidget(parent), owner_(owner)
{
	buildUi();
	syncFromPlugin();

	timer_ = new QTimer(this);
	timer_->setInterval(REFRESH_INTERVAL_MS);
	QObject::connect(timer_, &QTimer::timeout, [this]() { refresh(); });
	timer_->start();
}

void SceneDock::detach()
{
	detached_ = true;
	if (timer_)
		timer_->stop();
	owner_ = nullptr;
}

QString SceneDock::trText(const char *key) const
{
	const char *text = obs_module_text(key);
	return QString::fromUtf8(text && *text ? text : key);
}

/* ------------------------------------------------------------------------- */
/* the widget                                                                */
/* ------------------------------------------------------------------------- */

void SceneDock::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);

	sceneLabel_ = new QLabel(this);
	QFont bold = sceneLabel_->font();
	bold.setBold(true);
	sceneLabel_->setFont(bold);
	root->addWidget(sceneLabel_);

	summaryLabel_ = new QLabel(this);
	summaryLabel_->setWordWrap(true);
	root->addWidget(summaryLabel_);

	autoBox_ = new QCheckBox(trText("AutoScene.Dock.AutoRefit"), this);
	autoBox_->setToolTip(trText("AutoScene.Dock.AutoRefitHint"));
	QObject::connect(autoBox_, &QCheckBox::toggled, [this](bool on) {
		if (updating_ || !owner_)
			return;
		owner_->setAutoRefit(on);
	});
	root->addWidget(autoBox_);

	auto *options = new QGridLayout();
	options->addWidget(new QLabel(trText("AutoScene.Dock.Margin"), this), 0, 0);
	marginSpin_ = new QSpinBox(this);
	marginSpin_->setRange(0, 500);
	marginSpin_->setSuffix(trText("AutoScene.Dock.Pixels"));
	marginSpin_->setToolTip(trText("AutoScene.Dock.MarginHint"));
	options->addWidget(marginSpin_, 0, 1);

	options->addWidget(new QLabel(trText("AutoScene.Dock.Gap"), this), 0, 2);
	gapSpin_ = new QSpinBox(this);
	gapSpin_->setRange(0, 200);
	gapSpin_->setSuffix(trText("AutoScene.Dock.Pixels"));
	gapSpin_->setToolTip(trText("AutoScene.Dock.GapHint"));
	options->addWidget(gapSpin_, 0, 3);

	options->addWidget(new QLabel(trText("AutoScene.Dock.Anchor"), this), 1, 0);
	alignBox_ = new QComboBox(this);
	for (int i = 0; i < 9; i++) {
		const autoscene::Align align = autoscene::alignFromIndex(i);
		alignBox_->addItem(trText((std::string("AutoScene.Anchor.") + std::to_string(i)).c_str()),
				   QVariant(autoscene::alignIndex(align)));
	}
	options->addWidget(alignBox_, 1, 1);

	QObject::connect(marginSpin_, &QSpinBox::valueChanged, [this](int value) {
		if (!updating_ && owner_)
			owner_->setMargin(value);
	});
	QObject::connect(gapSpin_, &QSpinBox::valueChanged, [this](int value) {
		if (!updating_ && owner_)
			owner_->setGap(value);
	});
	QObject::connect(alignBox_, &QComboBox::currentIndexChanged, [this](int index) {
		if (!updating_ && owner_)
			owner_->setAlign(autoscene::alignFromIndex(index));
	});

	root->addLayout(options);

	table_ = new QTableWidget(0, ColumnCount, this);
	QStringList headers;
	headers << trText("AutoScene.Dock.Column.Source") << trText("AutoScene.Dock.Column.Size")
		<< trText("AutoScene.Dock.Column.Screen") << trText("AutoScene.Dock.Column.State")
		<< trText("AutoScene.Dock.Column.Mode");
	table_->setHorizontalHeaderLabels(headers);
	table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_->setSelectionMode(QAbstractItemView::SingleSelection);
	table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table_->setAlternatingRowColors(true);
	table_->verticalHeader()->setVisible(false);
	table_->horizontalHeader()->setStretchLastSection(true);
	table_->setColumnWidth(ColumnSource, 150);
	table_->setColumnWidth(ColumnSize, 90);
	table_->setColumnWidth(ColumnScreen, 130);
	table_->setColumnWidth(ColumnState, 90);
	root->addWidget(table_, 1);

	auto *buttons = new QHBoxLayout();
	fixButton_ = new QPushButton(trText("AutoScene.Dock.Button.FixAll"), this);
	fixButton_->setToolTip(trText("AutoScene.Dock.Button.FixAllHint"));
	QObject::connect(fixButton_, &QPushButton::clicked, [this]() {
		if (owner_) {
			owner_->fixScene();
			refresh();
		}
	});
	buttons->addWidget(fixButton_);

	gridButton_ = new QPushButton(trText("AutoScene.Dock.Button.Grid"), this);
	QObject::connect(gridButton_, &QPushButton::clicked, [this]() {
		if (owner_) {
			owner_->applyLayout(autoscene::Layout::Grid);
			refresh();
		}
	});
	buttons->addWidget(gridButton_);

	sideButton_ = new QPushButton(trText("AutoScene.Dock.Button.SideBySide"), this);
	QObject::connect(sideButton_, &QPushButton::clicked, [this]() {
		if (owner_) {
			owner_->applyLayout(autoscene::Layout::SideBySide);
			refresh();
		}
	});
	buttons->addWidget(sideButton_);

	cornerButton_ = new QPushButton(trText("AutoScene.Dock.Button.MainCorner"), this);
	QObject::connect(cornerButton_, &QPushButton::clicked, [this]() {
		if (owner_) {
			owner_->applyLayout(autoscene::Layout::MainCorner);
			refresh();
		}
	});
	buttons->addWidget(cornerButton_);

	undoButton_ = new QPushButton(trText("AutoScene.Dock.Button.Undo"), this);
	undoButton_->setToolTip(trText("AutoScene.Dock.Button.UndoHint"));
	QObject::connect(undoButton_, &QPushButton::clicked, [this]() {
		if (owner_) {
			owner_->undo();
			refresh();
		}
	});
	buttons->addWidget(undoButton_);

	root->addLayout(buttons);
}

/* ------------------------------------------------------------------------- */
/* filling it in                                                             */
/* ------------------------------------------------------------------------- */

QString SceneDock::stateText(int issueIndex, bool locked, bool hidden) const
{
	if (locked)
		return trText("AutoScene.State.Locked");
	if (hidden)
		return trText("AutoScene.State.Hidden");

	switch (static_cast<autoscene::Issue>(issueIndex)) {
	case autoscene::Issue::Fine:
		return trText("AutoScene.State.Fine");
	case autoscene::Issue::NoSize:
		return trText("AutoScene.State.NoSize");
	case autoscene::Issue::Rotated:
		return trText("AutoScene.State.Rotated");
	case autoscene::Issue::OffCanvas:
		return trText("AutoScene.State.OffCanvas");
	case autoscene::Issue::CutOff:
		return trText("AutoScene.State.CutOff");
	case autoscene::Issue::Tiny:
		return trText("AutoScene.State.Tiny");
	case autoscene::Issue::Distorted:
		return trText("AutoScene.State.Distorted");
	}

	return trText("AutoScene.State.Fine");
}

void SceneDock::syncFromPlugin()
{
	if (!owner_)
		return;

	updating_ = true;

	autoBox_->setChecked(owner_->config.autoRefit);
	marginSpin_->setValue(owner_->config.margin);
	gapSpin_->setValue(owner_->config.gap);
	alignBox_->setCurrentIndex(autoscene::alignIndex(owner_->config.align));

	updating_ = false;

	rowIds_.clear();
	rebuildRows();
	refresh();
}

void SceneDock::rebuildRows()
{
	if (!owner_)
		return;

	const std::vector<SceneRow> &rows = owner_->rows();

	updating_ = true;
	table_->clearContents();
	table_->setRowCount(static_cast<int>(rows.size()));

	rowIds_.clear();
	rowIds_.reserve(rows.size());

	for (size_t i = 0; i < rows.size(); i++) {
		const SceneRow &row = rows[i];
		const int index = static_cast<int>(i);
		rowIds_.push_back(row.id);

		table_->setItem(index, ColumnSource, new QTableWidgetItem(QString::fromStdString(row.name)));
		table_->setItem(index, ColumnSize, new QTableWidgetItem(QString()));
		table_->setItem(index, ColumnScreen, new QTableWidgetItem(QString()));
		table_->setItem(index, ColumnState, new QTableWidgetItem(QString()));

		auto *combo = new QComboBox(table_);
		combo->addItem(trText("AutoScene.Mode.Off"));
		combo->addItem(trText("AutoScene.Mode.Fit"));
		combo->addItem(trText("AutoScene.Mode.Fill"));
		combo->addItem(trText("AutoScene.Mode.Center"));
		combo->addItem(trText("AutoScene.Mode.Stretch"));
		combo->setCurrentIndex(indexForMode(row.mode));
		combo->setToolTip(trText("AutoScene.Dock.Column.ModeHint"));

		const int64_t id = row.id;
		/* No refresh() here: it could rebuild the table and delete the combo
		 * that is emitting this signal. The poll timer is a few hundred
		 * milliseconds away and will do it safely. */
		QObject::connect(combo, &QComboBox::currentIndexChanged, [this, id](int selection) {
			if (updating_ || !owner_)
				return;
			owner_->setMode(id, modeForIndex(selection));
		});

		table_->setCellWidget(index, ColumnMode, combo);
	}

	updating_ = false;
}

void SceneDock::updateRow(int index)
{
	if (!owner_)
		return;

	const std::vector<SceneRow> &rows = owner_->rows();
	if (index < 0 || static_cast<size_t>(index) >= rows.size())
		return;

	const SceneRow &row = rows[static_cast<size_t>(index)];

	if (QTableWidgetItem *item = table_->item(index, ColumnSource))
		item->setText(QString::fromStdString(row.name));

	if (QTableWidgetItem *item = table_->item(index, ColumnSize)) {
		if (row.srcW > 0 && row.srcH > 0)
			item->setText(QStringLiteral("%1×%2").arg(row.srcW).arg(row.srcH));
		else
			item->setText(trText("AutoScene.Dock.Column.NoSize"));
	}

	if (QTableWidgetItem *item = table_->item(index, ColumnScreen)) {
		if (row.srcW > 0 && row.srcH > 0) {
			item->setText(QStringLiteral("%1×%2 @ %3,%4")
					      .arg(row.rect.w)
					      .arg(row.rect.h)
					      .arg(row.rect.x)
					      .arg(row.rect.y));
		} else {
			item->setText(QString());
		}
	}

	if (QTableWidgetItem *item = table_->item(index, ColumnState))
		item->setText(stateText(static_cast<int>(row.issue), row.locked, !row.visible));

	auto *combo = static_cast<QComboBox *>(table_->cellWidget(index, ColumnMode));
	if (combo && !combo->hasFocus()) {
		const int wanted = indexForMode(row.mode);
		if (combo->currentIndex() != wanted) {
			updating_ = true;
			combo->setCurrentIndex(wanted);
			updating_ = false;
		}
	}
}

void SceneDock::refresh()
{
	if (detached_ || !owner_)
		return;

	sceneLabel_->setText(owner_->sceneText());
	summaryLabel_->setText(owner_->summaryText());

	autoBox_->setEnabled(true);
	undoButton_->setEnabled(owner_->canUndo());

	const std::vector<SceneRow> &rows = owner_->rows();

	bool sameRows = rows.size() == rowIds_.size();
	if (sameRows) {
		for (size_t i = 0; i < rows.size(); i++) {
			if (rows[i].id != rowIds_[i]) {
				sameRows = false;
				break;
			}
		}
	}

	if (!sameRows) {
		rebuildRows();
		return;
	}

	for (size_t i = 0; i < rows.size(); i++)
		updateRow(static_cast<int>(i));
}
