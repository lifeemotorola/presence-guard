/*
OBS Beautifier
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

#include <QWidget>

class Beautifier;

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

/**
 * The dock: presets, the accent colour, and the handful of switches that
 * change the look. Edits are debounced by a short timer, so dragging a slider
 * restyles OBS once at the end instead of on every step.
 */
class BeautifierDock : public QWidget {
public:
	explicit BeautifierDock(Beautifier *beautifier, QWidget *parent = nullptr);

	/* Re-reads everything from the beautifier, e.g. after OBS finished
	 * loading and the saved settings have been applied. */
	void syncFromBeautifier();

	/* Called from the module unload path: the beautifier can go away before
	 * the dock widget itself is destroyed by OBS. */
	void detach();

private:
	void buildUi();
	void scheduleApply();
	void pushOptions();
	void updateAccentSwatch();
	void updateValueLabels();

	QString trText(const char *key) const;

	Beautifier *beautifier_ = nullptr;
	bool loading_ = false;

	QCheckBox *enabledBox_ = nullptr;
	QComboBox *presetBox_ = nullptr;

	QCheckBox *autoAccentBox_ = nullptr;
	QPushButton *accentButton_ = nullptr;

	QSlider *radiusSlider_ = nullptr;
	QSlider *fontSlider_ = nullptr;
	QLabel *radiusValue_ = nullptr;
	QLabel *fontValue_ = nullptr;

	QComboBox *densityBox_ = nullptr;

	QCheckBox *roundedBox_ = nullptr;
	QCheckBox *tabsBox_ = nullptr;
	QCheckBox *scrollbarsBox_ = nullptr;
	QCheckBox *docksBox_ = nullptr;

	QPushButton *revertButton_ = nullptr;
	QPushButton *applyButton_ = nullptr;

	QLabel *statusLabel_ = nullptr;
	QLabel *detailLabel_ = nullptr;

	QTimer *applyTimer_ = nullptr;
};
