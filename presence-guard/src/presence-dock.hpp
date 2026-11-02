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

#pragma once

#include <QWidget>

class PresenceGuard;

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

/**
 * The dock widget. It is deliberately dumb: it renders PresenceGuard's
 * settings, pushes edits back, and polls the guard for a status line.
 */
class PresenceDock : public QWidget {
public:
	explicit PresenceDock(PresenceGuard *guard, QWidget *parent = nullptr);

	/* Re-reads scene/source lists and the guard's current settings. */
	void refreshFromGuard(void);

	/* Called from the module unload path: the guard may go away before the
	 * dock widget itself is destroyed by OBS, so drop every pointer to it. */
	void detach(void);

private:
	void buildUi(void);
	void populateScenes(void);
	void populateSources(void);
	void updateWidgetState(void);
	void pushToGuard(void);
	void refreshStatus(void);

	PresenceGuard *guard_ = nullptr;
	bool loading_ = false;

	QCheckBox *enabledBox_ = nullptr;
	QCheckBox *liveOnlyBox_ = nullptr;
	QCheckBox *deadAirBox_ = nullptr;

	QComboBox *afkSceneBox_ = nullptr;
	QComboBox *deadAirSceneBox_ = nullptr;
	QComboBox *micBox_ = nullptr;

	QSpinBox *afkTimeoutSpin_ = nullptr;
	QSpinBox *deadAirTimeoutSpin_ = nullptr;
	QSpinBox *thresholdSpin_ = nullptr;

	QGroupBox *afkGroup_ = nullptr;
	QGroupBox *deadAirGroup_ = nullptr;

	QLabel *statusLabel_ = nullptr;
	QLabel *hintLabel_ = nullptr;

	QPushButton *refreshButton_ = nullptr;
	QPushButton *testButton_ = nullptr;
	QPushButton *restoreButton_ = nullptr;

	QTimer *statusTimer_ = nullptr;
};
