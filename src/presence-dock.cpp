/*
Presence Guard for OBS Studio
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
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "presence-dock.hpp"
#include "presence-guard.hpp"
#include "platform-idle.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <string>

static QString trText(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* Collects every audio-capable input source name. */
struct SourceCollector {
	QComboBox *box = nullptr;
};

static bool collectAudioSources(void *param, obs_source_t *source)
{
	auto *collector = static_cast<SourceCollector *>(param);

	if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
		return true;

	const char *name = obs_source_get_name(source);
	if (!name || !*name)
		return true;

	collector->box->addItem(QString::fromUtf8(name), QString::fromUtf8(name));
	return true;
}

PresenceDock::PresenceDock(PresenceGuard *guard, QWidget *parent) : QWidget(parent), guard_(guard)
{
	buildUi();
	refreshFromGuard();

	statusTimer_ = new QTimer(this);
	statusTimer_->setInterval(500);
	QObject::connect(statusTimer_, &QTimer::timeout, this, [this]() { refreshStatus(); });
	statusTimer_->start();

	refreshStatus();
}

void PresenceDock::detach(void)
{
	if (statusTimer_)
		statusTimer_->stop();

	guard_ = nullptr;
	enabledBox_->setEnabled(false);
}

void PresenceDock::buildUi(void)
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	enabledBox_ = new QCheckBox(trText("PresenceGuard.Enable"), this);
	root->addWidget(enabledBox_);

	liveOnlyBox_ = new QCheckBox(trText("PresenceGuard.OnlyWhileLive"), this);
	root->addWidget(liveOnlyBox_);

	/* --- AFK ------------------------------------------------------------ */

	afkGroup_ = new QGroupBox(trText("PresenceGuard.AfkGroup"), this);
	auto *afkForm = new QFormLayout(afkGroup_);
	afkSceneBox_ = new QComboBox(afkGroup_);
	afkForm->addRow(trText("PresenceGuard.AfkScene"), afkSceneBox_);
	afkTimeoutSpin_ = new QSpinBox(afkGroup_);
	afkTimeoutSpin_->setRange(10, 3600);
	afkTimeoutSpin_->setSuffix(" s");
	afkForm->addRow(trText("PresenceGuard.AfkAfter"), afkTimeoutSpin_);
	root->addWidget(afkGroup_);

	/* --- dead air -------------------------------------------------------- */

	deadAirBox_ = new QCheckBox(trText("PresenceGuard.DeadAirEnable"), this);
	root->addWidget(deadAirBox_);

	deadAirGroup_ = new QGroupBox(trText("PresenceGuard.DeadAirGroup"), this);
	auto *deadForm = new QFormLayout(deadAirGroup_);
	deadAirSceneBox_ = new QComboBox(deadAirGroup_);
	deadForm->addRow(trText("PresenceGuard.DeadAirScene"), deadAirSceneBox_);
	micBox_ = new QComboBox(deadAirGroup_);
	deadForm->addRow(trText("PresenceGuard.Microphone"), micBox_);
	deadAirTimeoutSpin_ = new QSpinBox(deadAirGroup_);
	deadAirTimeoutSpin_->setRange(5, 1800);
	deadAirTimeoutSpin_->setSuffix(" s");
	deadForm->addRow(trText("PresenceGuard.DeadAirAfter"), deadAirTimeoutSpin_);
	thresholdSpin_ = new QSpinBox(deadAirGroup_);
	thresholdSpin_->setRange(-90, 0);
	thresholdSpin_->setSuffix(" dB");
	deadForm->addRow(trText("PresenceGuard.Threshold"), thresholdSpin_);
	root->addWidget(deadAirGroup_);

	/* --- buttons --------------------------------------------------------- */

	auto *buttons = new QHBoxLayout();
	refreshButton_ = new QPushButton(trText("PresenceGuard.Refresh"), this);
	testButton_ = new QPushButton(trText("PresenceGuard.TestAfk"), this);
	restoreButton_ = new QPushButton(trText("PresenceGuard.Restore"), this);
	buttons->addWidget(refreshButton_);
	buttons->addWidget(testButton_);
	buttons->addWidget(restoreButton_);
	root->addLayout(buttons);

	/* --- status ---------------------------------------------------------- */

	statusLabel_ = new QLabel(this);
	statusLabel_->setWordWrap(true);
	root->addWidget(statusLabel_);

	hintLabel_ = new QLabel(this);
	hintLabel_->setWordWrap(true);
	hintLabel_->setStyleSheet("color: palette(mid);");
	root->addWidget(hintLabel_);

	root->addStretch(1);

	QString hint = trText("PresenceGuard.Hint");
	if (!pg_idle_supported())
		hint = trText("PresenceGuard.IdleUnsupported") + " " + hint;
	hintLabel_->setText(hint);

	updateWidgetState();

	/* --- wiring ---------------------------------------------------------- */

	QObject::connect(enabledBox_, &QCheckBox::toggled, this, [this](bool) {
		updateWidgetState();
		pushToGuard();
	});
	QObject::connect(liveOnlyBox_, &QCheckBox::toggled, this, [this](bool) { pushToGuard(); });
	QObject::connect(deadAirBox_, &QCheckBox::toggled, this, [this](bool) {
		updateWidgetState();
		pushToGuard();
	});
	QObject::connect(afkSceneBox_, &QComboBox::currentIndexChanged, this, [this](int) { pushToGuard(); });
	QObject::connect(deadAirSceneBox_, &QComboBox::currentIndexChanged, this, [this](int) { pushToGuard(); });
	QObject::connect(micBox_, &QComboBox::currentIndexChanged, this, [this](int) { pushToGuard(); });
	QObject::connect(afkTimeoutSpin_, &QSpinBox::valueChanged, this, [this](int) { pushToGuard(); });
	QObject::connect(deadAirTimeoutSpin_, &QSpinBox::valueChanged, this, [this](int) { pushToGuard(); });
	QObject::connect(thresholdSpin_, &QSpinBox::valueChanged, this, [this](int) { pushToGuard(); });

	QObject::connect(refreshButton_, &QPushButton::clicked, this, [this]() {
		populateScenes();
		populateSources();
		refreshStatus();
	});

	QObject::connect(testButton_, &QPushButton::clicked, this, [this]() {
		if (guard_)
			guard_->triggerAfkNow();
		refreshStatus();
	});

	QObject::connect(restoreButton_, &QPushButton::clicked, this, [this]() {
		if (guard_)
			guard_->restoreNow();
		refreshStatus();
	});
}

void PresenceDock::refreshFromGuard(void)
{
	populateScenes();
	populateSources();

	if (!guard_)
		return;

	loading_ = true;
	enabledBox_->setChecked(guard_->settings.enabled);
	liveOnlyBox_->setChecked(guard_->settings.onlyWhileLive);
	deadAirBox_->setChecked(guard_->settings.deadAirEnabled);

	afkTimeoutSpin_->setValue(guard_->settings.afkTimeoutSec);
	deadAirTimeoutSpin_->setValue(guard_->settings.deadAirTimeoutSec);
	thresholdSpin_->setValue(guard_->settings.thresholdDb);

	int index = afkSceneBox_->findData(QString::fromStdString(guard_->settings.afkScene));
	afkSceneBox_->setCurrentIndex(index >= 0 ? index : 0);

	index = deadAirSceneBox_->findData(QString::fromStdString(guard_->settings.deadAirScene));
	deadAirSceneBox_->setCurrentIndex(index >= 0 ? index : 0);

	index = micBox_->findData(QString::fromStdString(guard_->settings.micSource));
	micBox_->setCurrentIndex(index >= 0 ? index : 0);
	loading_ = false;

	updateWidgetState();
	refreshStatus();
}

void PresenceDock::populateScenes(void)
{
	const QSignalBlocker afkBlocker(afkSceneBox_);
	const QSignalBlocker deadBlocker(deadAirSceneBox_);

	const QString afkCurrent = afkSceneBox_->currentData().toString();
	const QString deadCurrent = deadAirSceneBox_->currentData().toString();

	afkSceneBox_->clear();
	deadAirSceneBox_->clear();

	afkSceneBox_->addItem(trText("PresenceGuard.NoScene"), QString());
	deadAirSceneBox_->addItem(trText("PresenceGuard.SameAsAfk"), QString());

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		const char *name = obs_source_get_name(scenes.sources.array[i]);
		if (!name || !*name)
			continue;

		const QString value = QString::fromUtf8(name);
		afkSceneBox_->addItem(value, value);
		deadAirSceneBox_->addItem(value, value);
	}

	obs_frontend_source_list_free(&scenes);

	int index = afkSceneBox_->findData(
		afkCurrent.isEmpty() && guard_ ? QString::fromStdString(guard_->settings.afkScene) : afkCurrent);
	afkSceneBox_->setCurrentIndex(index >= 0 ? index : 0);

	index = deadAirSceneBox_->findData(
		deadCurrent.isEmpty() && guard_ ? QString::fromStdString(guard_->settings.deadAirScene) : deadCurrent);
	deadAirSceneBox_->setCurrentIndex(index >= 0 ? index : 0);
}

void PresenceDock::populateSources(void)
{
	const QSignalBlocker blocker(micBox_);

	const QString current = micBox_->currentData().toString().isEmpty() && guard_
					? QString::fromStdString(guard_->settings.micSource)
					: micBox_->currentData().toString();

	micBox_->clear();
	micBox_->addItem(trText("PresenceGuard.NoMicrophone"), QString());

	SourceCollector collector;
	collector.box = micBox_;
	obs_enum_sources(collectAudioSources, &collector);

	const int index = micBox_->findData(current);
	micBox_->setCurrentIndex(index >= 0 ? index : 0);
}

void PresenceDock::updateWidgetState(void)
{
	const bool enabled = enabledBox_->isChecked();
	liveOnlyBox_->setEnabled(enabled);
	afkGroup_->setEnabled(enabled);
	deadAirBox_->setEnabled(enabled);
	deadAirGroup_->setEnabled(enabled && deadAirBox_->isChecked());
	refreshButton_->setEnabled(true);
	testButton_->setEnabled(enabled);
	restoreButton_->setEnabled(enabled);
}

void PresenceDock::pushToGuard(void)
{
	if (loading_ || !guard_)
		return;

	PresenceSettings next = guard_->settings;
	next.enabled = enabledBox_->isChecked();
	next.onlyWhileLive = liveOnlyBox_->isChecked();
	next.deadAirEnabled = deadAirBox_->isChecked();
	next.afkTimeoutSec = afkTimeoutSpin_->value();
	next.deadAirTimeoutSec = deadAirTimeoutSpin_->value();
	next.thresholdDb = thresholdSpin_->value();
	next.afkScene = afkSceneBox_->currentData().toString().toStdString();
	next.deadAirScene = deadAirSceneBox_->currentData().toString().toStdString();
	next.micSource = micBox_->currentData().toString().toStdString();

	guard_->applySettings(next);
	guard_->saveNow();

	refreshStatus();
}

void PresenceDock::refreshStatus(void)
{
	if (!guard_) {
		statusLabel_->setText(trText("PresenceGuard.Status.Detached"));
		return;
	}

	statusLabel_->setText(guard_->statusText());

	switch (guard_->state()) {
	case GuardState::Afk:
	case GuardState::DeadAir:
		statusLabel_->setStyleSheet("color: #ff8a3d; font-weight: 600;");
		break;
	default:
		statusLabel_->setStyleSheet("");
		break;
	}
}
