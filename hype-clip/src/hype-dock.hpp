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

#pragma once

#include <QString>
#include <QWidget>

#include <cstdint>

class HypeClip;

class QCheckBox;
class QGridLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

/**
 * The dock: one big "Clip that" button, the rules for what counts as a moment,
 * a live meter, and the clips that came out of it.
 */
class HypeDock : public QWidget {
public:
	explicit HypeDock(HypeClip *owner, QWidget *parent = nullptr);

	/* Re-reads the rules from the plugin. */
	void syncFromPlugin();

	/* Called from the module unload path: the plugin can go away before the
	 * dock widget itself is destroyed by OBS. */
	void detach();

private:
	void buildUi();
	void pushConfig();
	void refreshStatus();
	void refreshClips();

	QString trText(const char *key) const;
	QString selectedClipPath() const;

	HypeClip *owner_ = nullptr;
	bool loading_ = false;

	QCheckBox *autoClipBox_ = nullptr;
	QPushButton *clipButton_ = nullptr;

	QLabel *statusLabel_ = nullptr;
	QLabel *detailLabel_ = nullptr;
	QProgressBar *meterBar_ = nullptr;

	QSpinBox *thresholdSpin_ = nullptr;
	QSpinBox *marginSpin_ = nullptr;
	QSpinBox *sustainSpin_ = nullptr;
	QSpinBox *cooldownSpin_ = nullptr;

	QTableWidget *clipTable_ = nullptr;
	QPushButton *folderButton_ = nullptr;
	QPushButton *copyButton_ = nullptr;
	QPushButton *deleteButton_ = nullptr;
	QPushButton *clearButton_ = nullptr;

	QTimer *statusTimer_ = nullptr;

	/* What the clip table was last built from, so it is only rebuilt when the
	 * list actually changed (and selections survive). */
	int64_t lastClipStamp_ = -1;
	int lastClipCount_ = -1;
};
