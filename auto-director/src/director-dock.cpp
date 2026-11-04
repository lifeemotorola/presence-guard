/*
Auto Director for OBS Studio
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

#include "director-dock.hpp"
#include "auto-director.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QFont>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

static const char *EMPTY_SLOT = "<none>";

static QString trText(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

static QString slotText()
{
	return QString::fromUtf8(EMPTY_SLOT);
}

/* Looks for a microphone inside a scene, so "Add all scenes" can pair a camera
 * with its audio in one click. Prefers audio-only sources (microphones) over
 * cameras, because a camera's own audio is usually the one we do not want. */
static bool pickAudioSource(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	std::string *out = static_cast<std::string *>(param);
	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source || !obs_sceneitem_visible(item))
		return true;

	const uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_AUDIO) == 0)
		return true;

	const char *name = obs_source_get_name(source);
	if (!name || !*name)
		return true;

	/* A microphone output only audio: that is the speaker's own source. */
	if ((flags & OBS_SOURCE_VIDEO) != 0 && !out->empty())
		return true;

	*out = name;
	return (flags & OBS_SOURCE_VIDEO) == 0;
}

/* The microphone inside a scene, if it has one. */
static std::string audioInsideScene(const std::string &sceneName)
{
	std::string out;

	obs_source_t *scene = sceneName.empty() ? nullptr : obs_get_source_by_name(sceneName.c_str());
	if (scene) {
		obs_scene_t *sceneData = obs_scene_from_source(scene);
		if (sceneData)
			obs_scene_enum_items(sceneData, pickAudioSource, &out);
		obs_source_release(scene);
	}

	return out;
}

DirectorDock::DirectorDock(Director *director, QWidget *parent) : QWidget(parent), director_(director)
{
	buildUi();
	reloadLists();

	statusTimer_ = new QTimer(this);
	statusTimer_->setInterval(250);
	QObject::connect(statusTimer_, &QTimer::timeout, [this]() { refreshStatus(); });
	statusTimer_->start();

	refreshStatus();
}

/* ------------------------------------------------------------------------- */
/* construction                                                              */
/* ------------------------------------------------------------------------- */

void DirectorDock::populateCombo(QComboBox *box, const QStringList &items)
{
	const QString current = box->currentText();

	box->blockSignals(true);
	box->clear();
	box->addItems(items);
	const int index = box->findText(current);
	if (index >= 0)
		box->setCurrentIndex(index);
	box->blockSignals(false);
}

QStringList DirectorDock::sceneNames() const
{
	QStringList names;

	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		const char *name = obs_source_get_name(scenes.sources.array[i]);
		if (name && *name)
			names << QString::fromUtf8(name);
	}
	obs_frontend_source_list_free(&scenes);

	return names;
}

QStringList DirectorDock::sourceNames() const
{
	QStringList names;
	names << slotText();

	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);

	QStringList seen;
	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_source_t *scene = scenes.sources.array[i];
		obs_scene_t *sceneData = obs_scene_from_source(scene);
		if (!sceneData)
			continue;

		obs_scene_enum_items(
			sceneData,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
				QStringList *list = static_cast<QStringList *>(param);
				obs_source_t *source = obs_sceneitem_get_source(item);
				if (!source)
					return true;

				const uint32_t flags = obs_source_get_output_flags(source);
				if ((flags & OBS_SOURCE_AUDIO) == 0)
					return true;

				const char *name = obs_source_get_name(source);
				if (name && *name && !list->contains(QString::fromUtf8(name)))
					*list << QString::fromUtf8(name);

				return true;
			},
			&seen);
	}

	obs_frontend_source_list_free(&scenes);

	seen.sort(Qt::CaseInsensitive);
	return names + seen;
}

void DirectorDock::buildUi()
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(6);

	enabledBox_ = new QCheckBox(trText("AutoDirector.Enabled"), this);
	enabledBox_->setToolTip(trText("AutoDirector.Enabled.Tooltip"));
	layout->addWidget(enabledBox_);

	dryRunBox_ = new QCheckBox(trText("AutoDirector.DryRun"), this);
	dryRunBox_->setToolTip(trText("AutoDirector.DryRun.Tooltip"));
	layout->addWidget(dryRunBox_);

	liveOnlyBox_ = new QCheckBox(trText("AutoDirector.LiveOnly"), this);
	liveOnlyBox_->setToolTip(trText("AutoDirector.LiveOnly.Tooltip"));
	layout->addWidget(liveOnlyBox_);

	QGroupBox *statusGroup = new QGroupBox(trText("AutoDirector.Status"), this);
	QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
	statusLabel_ = new QLabel(statusGroup);
	QFont bold = statusLabel_->font();
	bold.setBold(true);
	statusLabel_->setFont(bold);
	detailLabel_ = new QLabel(statusGroup);
	detailLabel_->setWordWrap(true);
	statusLayout->addWidget(statusLabel_);
	statusLayout->addWidget(detailLabel_);

	layout->addWidget(statusGroup);

	takesGroup_ = new QGroupBox(trText("AutoDirector.Takes"), this);
	QVBoxLayout *takesLayout = new QVBoxLayout(takesGroup_);

	table_ = new QTableWidget(0, 4, takesGroup_);
	table_->setHorizontalHeaderLabels(
		QStringList() << trText("AutoDirector.Column.On") << trText("AutoDirector.Column.Scene")
			      << trText("AutoDirector.Column.Microphone") << trText("AutoDirector.Column.Level"));
	table_->verticalHeader()->setVisible(false);
	table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	table_->setMinimumHeight(120);
	takesLayout->addWidget(table_);

	QHBoxLayout *buttons = new QHBoxLayout();
	addButton_ = new QPushButton(trText("AutoDirector.AddTake"), takesGroup_);
	addAllButton_ = new QPushButton(trText("AutoDirector.AddAll"), takesGroup_);
	addAllButton_->setToolTip(trText("AutoDirector.AddAll.Tooltip"));
	removeButton_ = new QPushButton(trText("AutoDirector.RemoveTake"), takesGroup_);
	buttons->addWidget(addButton_);
	buttons->addWidget(addAllButton_);
	buttons->addWidget(removeButton_);
	takesLayout->addLayout(buttons);

	QHBoxLayout *controls = new QHBoxLayout();
	nextButton_ = new QPushButton(trText("AutoDirector.Next"), takesGroup_);
	nextButton_->setToolTip(trText("AutoDirector.Next.Tooltip"));
	lockButton_ = new QPushButton(trText("AutoDirector.Hold"), takesGroup_);
	lockButton_->setToolTip(trText("AutoDirector.Hold.Tooltip"));
	lobbyButton_ = new QPushButton(trText("AutoDirector.Lobby"), takesGroup_);
	lobbyButton_->setToolTip(trText("AutoDirector.Lobby.Tooltip"));
	controls->addWidget(nextButton_);
	controls->addWidget(lockButton_);
	controls->addWidget(lobbyButton_);
	takesLayout->addLayout(controls);

	layout->addWidget(takesGroup_);

	QGroupBox *tuningGroup = new QGroupBox(trText("AutoDirector.Tuning"), this);
	QGridLayout *grid = new QGridLayout(tuningGroup);
	grid->setColumnStretch(1, 1);

	int row = 0;
	auto addSpin = [&](const char *labelText, const char *suffix, int min, int max, int step, QSpinBox **out) {
		QLabel *label = new QLabel(trText(labelText), tuningGroup);
		QSpinBox *spin = new QSpinBox(tuningGroup);
		spin->setRange(min, max);
		spin->setSingleStep(step);
		spin->setSuffix(suffix ? QString::fromUtf8(suffix) : QString());
		label->setBuddy(spin);
		grid->addWidget(label, row, 0);
		grid->addWidget(spin, row, 1);
		row++;
		*out = spin;
	};

	addSpin("AutoDirector.Threshold", " dB", -90, 0, 1, &thresholdSpin_);
	addSpin("AutoDirector.Hysteresis", " dB", 0, 30, 1, &hysteresisSpin_);
	addSpin("AutoDirector.Dwell", " ms", 0, 60000, 250, &dwellSpin_);
	addSpin("AutoDirector.FallbackDelay", " s", 0, 3600, 5, &fallbackDelaySpin_);
	addSpin("AutoDirector.Lock", " s", 0, 600, 1, &lockSpin_);

	QLabel *fallbackLabel = new QLabel(trText("AutoDirector.FallbackScene"), tuningGroup);
	fallbackBox_ = new QComboBox(tuningGroup);
	fallbackBox_->setToolTip(trText("AutoDirector.FallbackScene.Tooltip"));
	fallbackLabel->setBuddy(fallbackBox_);
	grid->addWidget(fallbackLabel, row, 0);
	grid->addWidget(fallbackBox_, row, 1);

	layout->addWidget(tuningGroup);

	hintLabel_ = new QLabel(trText("AutoDirector.Hint"), this);
	hintLabel_->setWordWrap(true);
	hintLabel_->setEnabled(false);
	layout->addWidget(hintLabel_);

	layout->addStretch(1);

	/* --- wiring --- */

	QObject::connect(enabledBox_, &QCheckBox::toggled, [this](bool on) {
		if (loading_ || !director_)
			return;
		DirectorSettings next = director_->settings;
		next.enabled = on;
		director_->applySettings(next);
	});

	QObject::connect(dryRunBox_, &QCheckBox::toggled, [this](bool on) {
		if (loading_ || !director_)
			return;
		DirectorSettings next = director_->settings;
		next.dryRun = on;
		director_->applySettings(next);
	});

	QObject::connect(liveOnlyBox_, &QCheckBox::toggled, [this](bool on) {
		if (loading_ || !director_)
			return;
		DirectorSettings next = director_->settings;
		next.onlyWhileLive = on;
		director_->applySettings(next);
	});

	auto pushNumbers = [this]() {
		if (loading_ || !director_)
			return;
		DirectorSettings next = director_->settings;
		next.thresholdDb = thresholdSpin_->value();
		next.hysteresisDb = hysteresisSpin_->value();
		next.dwellMs = dwellSpin_->value();
		next.fallbackDelaySec = fallbackDelaySpin_->value();
		next.operatorLockMs = lockSpin_->value() * 1000;
		next.fallbackScene = fallbackBox_->currentIndex() <= 0 ? std::string()
								       : fallbackBox_->currentText().toStdString();
		director_->applySettings(next);
	};

	QObject::connect(thresholdSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
			 [pushNumbers](int) { pushNumbers(); });
	QObject::connect(hysteresisSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
			 [pushNumbers](int) { pushNumbers(); });
	QObject::connect(dwellSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
			 [pushNumbers](int) { pushNumbers(); });
	QObject::connect(fallbackDelaySpin_, QOverload<int>::of(&QSpinBox::valueChanged),
			 [pushNumbers](int) { pushNumbers(); });
	QObject::connect(lockSpin_, QOverload<int>::of(&QSpinBox::valueChanged), [pushNumbers](int) { pushNumbers(); });
	QObject::connect(fallbackBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
			 [pushNumbers](int) { pushNumbers(); });

	QObject::connect(addButton_, &QPushButton::clicked, [this]() {
		const QString scene = sceneNames().value(0);
		addTake(scene, QString::fromStdString(audioInsideScene(scene.toStdString())));
		reloadTable();
	});

	QObject::connect(addAllButton_, &QPushButton::clicked, [this]() {
		if (!director_)
			return;

		obs_frontend_source_list scenes = {};
		obs_frontend_get_scenes(&scenes);

		std::vector<director::Take> takes = director_->settings.takes;

		for (size_t i = 0; i < scenes.sources.num; i++) {
			obs_source_t *scene = scenes.sources.array[i];
			const char *name = obs_source_get_name(scene);
			if (!name || !*name)
				continue;

			const std::string sceneName(name);
			bool already = false;
			for (const director::Take &take : takes) {
				if (take.scene == sceneName)
					already = true;
			}
			if (already)
				continue;

			director::Take take;
			take.scene = sceneName;
			take.source = audioInsideScene(sceneName);
			take.enabled = true;
			takes.push_back(take);
		}

		obs_frontend_source_list_free(&scenes);

		DirectorSettings next = director_->settings;
		next.takes = takes;
		director_->applySettings(next);
		reloadTable();
	});

	QObject::connect(removeButton_, &QPushButton::clicked, [this]() {
		if (!director_)
			return;

		const int row = table_->currentRow();
		if (row < 0 || row >= (int)director_->settings.takes.size())
			return;

		DirectorSettings next = director_->settings;
		next.takes.erase(next.takes.begin() + row);
		director_->applySettings(next);
		reloadTable();
	});

	QObject::connect(nextButton_, &QPushButton::clicked, [this]() {
		if (director_)
			director_->nextTake();
	});
	QObject::connect(lockButton_, &QPushButton::clicked, [this]() {
		if (director_)
			director_->lockNow();
	});
	QObject::connect(lobbyButton_, &QPushButton::clicked, [this]() {
		if (director_)
			director_->fallbackNow();
	});
}

/* ------------------------------------------------------------------------- */
/* table                                                                     */
/* ------------------------------------------------------------------------- */

void DirectorDock::addTake(const QString &scene, const QString &source)
{
	if (!director_)
		return;

	DirectorSettings next = director_->settings;

	director::Take take;
	take.scene = scene.toStdString();
	take.source = source == slotText() ? std::string() : source.toStdString();
	take.enabled = true;

	next.takes.push_back(take);
	director_->applySettings(next);
}

void DirectorDock::reloadTable()
{
	if (!director_)
		return;

	loading_ = true;

	const QStringList scenes = sceneNames();
	const QStringList sources = sourceNames();

	table_->setRowCount(0);

	const std::vector<director::Take> &takes = director_->settings.takes;
	table_->setRowCount((int)takes.size());

	for (int row = 0; row < (int)takes.size(); row++) {
		const director::Take &take = takes[(size_t)row];

		QCheckBox *on = new QCheckBox(table_);
		on->setChecked(take.enabled);
		QObject::connect(on, &QCheckBox::toggled, [this, row](bool checked) {
			if (loading_ || !director_)
				return;
			if (row >= (int)director_->settings.takes.size())
				return;
			DirectorSettings next = director_->settings;
			next.takes[(size_t)row].enabled = checked;
			director_->applySettings(next);
		});

		QWidget *onCell = new QWidget(table_);
		QHBoxLayout *onLayout = new QHBoxLayout(onCell);
		onLayout->setContentsMargins(0, 0, 0, 0);
		onLayout->setAlignment(Qt::AlignCenter);
		onLayout->addWidget(on);
		table_->setCellWidget(row, 0, onCell);

		QComboBox *sceneBox = new QComboBox(table_);
		sceneBox->addItems(scenes);
		const QString sceneName = QString::fromStdString(take.scene);
		if (!sceneName.isEmpty() && sceneBox->findText(sceneName) < 0)
			sceneBox->addItem(sceneName);
		sceneBox->setCurrentText(sceneName);
		QObject::connect(sceneBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
				 [this, row, sceneBox](int) {
					 if (loading_ || !director_)
						 return;
					 if (row >= (int)director_->settings.takes.size())
						 return;
					 DirectorSettings next = director_->settings;
					 next.takes[(size_t)row].scene = sceneBox->currentText().toStdString();
					 director_->applySettings(next);
				 });
		table_->setCellWidget(row, 1, sceneBox);

		QComboBox *sourceBox = new QComboBox(table_);
		sourceBox->addItems(sources);
		const QString sourceName = take.source.empty() ? slotText() : QString::fromStdString(take.source);
		if (sourceBox->findText(sourceName) < 0)
			sourceBox->addItem(sourceName);
		sourceBox->setCurrentText(sourceName);
		QObject::connect(sourceBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
				 [this, row, sourceBox](int) {
					 if (loading_ || !director_)
						 return;
					 if (row >= (int)director_->settings.takes.size())
						 return;
					 DirectorSettings next = director_->settings;
					 const QString chosen = sourceBox->currentText();
					 next.takes[(size_t)row].source = chosen == slotText() ? std::string()
											       : chosen.toStdString();
					 director_->applySettings(next);
				 });
		table_->setCellWidget(row, 2, sourceBox);

		QLabel *level = new QLabel(table_);
		level->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		table_->setCellWidget(row, 3, level);
	}

	loading_ = false;

	syncFromDirector();
}

void DirectorDock::reloadLists()
{
	reloadTable();

	if (!director_)
		return;

	loading_ = true;

	const QStringList scenes = sceneNames();
	populateCombo(fallbackBox_, QStringList() << slotText() << scenes);

	loading_ = false;
	syncFromDirector();
}

void DirectorDock::syncFromDirector()
{
	if (!director_ || loading_)
		return;

	loading_ = true;

	const DirectorSettings &settings = director_->settings;

	enabledBox_->setChecked(settings.enabled);
	dryRunBox_->setChecked(settings.dryRun);
	liveOnlyBox_->setChecked(settings.onlyWhileLive);

	thresholdSpin_->setValue(settings.thresholdDb);
	hysteresisSpin_->setValue(settings.hysteresisDb);
	dwellSpin_->setValue(settings.dwellMs);
	fallbackDelaySpin_->setValue(settings.fallbackDelaySec);
	lockSpin_->setValue(settings.operatorLockMs / 1000);

	const QString fallback = settings.fallbackScene.empty() ? slotText()
								: QString::fromStdString(settings.fallbackScene);
	if (fallbackBox_->findText(fallback) < 0)
		fallbackBox_->addItem(fallback);
	const int index = fallbackBox_->findText(fallback);
	fallbackBox_->setCurrentIndex(index >= 0 ? index : 0);

	loading_ = false;
}

void DirectorDock::detach()
{
	director_ = nullptr;

	if (statusTimer_)
		statusTimer_->stop();
}

void DirectorDock::refreshStatus()
{
	if (!director_)
		return;

	statusLabel_->setText(director_->statusText());
	detailLabel_->setText(director_->detailText());

	/* Hotkeys can flip these behind the dock's back. */
	if (enabledBox_->isChecked() != director_->settings.enabled) {
		loading_ = true;
		enabledBox_->setChecked(director_->settings.enabled);
		loading_ = false;
	}

	const std::vector<director::Take> &takes = director_->settings.takes;
	for (int row = 0; row < (int)takes.size(); row++) {
		QLabel *level = qobject_cast<QLabel *>(table_->cellWidget(row, 3));
		if (!level)
			continue;

		if (!director_->levelAlive((size_t)row)) {
			level->setText(trText("AutoDirector.Level.Silent"));
			level->setToolTip(trText("AutoDirector.Level.Silent.Tooltip"));
			continue;
		}

		const float db = director_->levelDb((size_t)row);
		level->setText(QStringLiteral("%1 dB").arg((int)(db < -99.0f ? -99.0f : db)));
		level->setToolTip(QString());
	}
}
