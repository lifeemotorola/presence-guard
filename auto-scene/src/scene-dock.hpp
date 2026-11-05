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

#pragma once

#include <QString>
#include <QWidget>

#include <cstdint>
#include <vector>

class SceneAdjust;

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

/**
 * The dock: what is on screen, what is wrong with it, and the three ways to
 * fix it — one source at a time, the whole scene, or a layout preset.
 */
class SceneDock : public QWidget {
public:
	explicit SceneDock(SceneAdjust *owner, QWidget *parent = nullptr);

	/* Re-reads the settings and the scene list. */
	void syncFromPlugin();

	/* Called from the module unload path: the plugin can go away before the
	 * dock widget itself is destroyed by OBS. */
	void detach();

private:
	void buildUi();
	void refresh();
	void rebuildRows();
	void updateRow(int row);
	void pushConfig();

	QString trText(const char *key) const;
	QString stateText(int issueIndex, bool locked, bool hidden) const;

	SceneAdjust *owner_ = nullptr;

	QLabel *sceneLabel_ = nullptr;
	QLabel *summaryLabel_ = nullptr;
	QCheckBox *autoBox_ = nullptr;
	QSpinBox *marginSpin_ = nullptr;
	QSpinBox *gapSpin_ = nullptr;
	QComboBox *alignBox_ = nullptr;
	QTableWidget *table_ = nullptr;

	QPushButton *fixButton_ = nullptr;
	QPushButton *gridButton_ = nullptr;
	QPushButton *sideButton_ = nullptr;
	QPushButton *cornerButton_ = nullptr;
	QPushButton *undoButton_ = nullptr;

	QTimer *timer_ = nullptr;

	/* The rows currently in the table, so a rebuild only happens when the
	 * scene itself changed. */
	std::vector<int64_t> rowIds_;

	bool updating_ = false;
	bool detached_ = false;
};
