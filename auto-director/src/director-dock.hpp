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

#pragma once

#include <QStringList>
#include <QWidget>

class Director;

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

/**
 * The dock. It is deliberately dumb: it renders Director's settings, pushes
 * edits back through the director, and shows what the director is doing. All
 * of the decision making stays in the director so that the hotkeys and the
 * dock behave identically.
 */
class DirectorDock : public QWidget {
public:
	explicit DirectorDock(Director *director, QWidget *parent = nullptr);

	/* Re-reads the scene and source lists from OBS. */
	void reloadLists();

	/* Called from the module unload path: the director can go away before
	 * the dock widget itself is destroyed by OBS. */
	void detach();

private:
	void buildUi();
	void reloadTable();
	void collectTable();
	void syncFromDirector();
	void refreshStatus();
	void addTake(const QString &scene, const QString &source);
	QStringList sceneNames() const;
	QStringList sourceNames() const;

	void populateCombo(QComboBox *box, const QStringList &items);

	Director *director_ = nullptr;
	bool loading_ = false;

	QCheckBox *enabledBox_ = nullptr;
	QCheckBox *dryRunBox_ = nullptr;
	QCheckBox *liveOnlyBox_ = nullptr;

	QGroupBox *takesGroup_ = nullptr;
	QTableWidget *table_ = nullptr;

	QPushButton *addButton_ = nullptr;
	QPushButton *addAllButton_ = nullptr;
	QPushButton *removeButton_ = nullptr;
	QPushButton *nextButton_ = nullptr;
	QPushButton *lockButton_ = nullptr;
	QPushButton *lobbyButton_ = nullptr;

	QComboBox *fallbackBox_ = nullptr;
	QSpinBox *thresholdSpin_ = nullptr;
	QSpinBox *hysteresisSpin_ = nullptr;
	QSpinBox *dwellSpin_ = nullptr;
	QSpinBox *fallbackDelaySpin_ = nullptr;
	QSpinBox *lockSpin_ = nullptr;

	QLabel *statusLabel_ = nullptr;
	QLabel *detailLabel_ = nullptr;
	QLabel *hintLabel_ = nullptr;

	QTimer *statusTimer_ = nullptr;
};
