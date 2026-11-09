/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#pragma once

#include "twin-core.hpp"

#include <QWidget>
#include <optional>

class QComboBox;
class QLabel;
class QPushButton;
class QTextBrowser;

class TwinDock : public QWidget {
public:
	explicit TwinDock(QWidget *parent = nullptr);
	void invalidate();

private:
	void buildUi();
	bool scan(bool keepSelection);
	void loadBaseline();
	void saveBaseline();
	void simulateFailure();
	void exportReport();
	void refreshReport();
	void populateSources(const QString &primaryId, const QString &backupId);
	void clearSimulation();
	QString baselinePath() const;
	QString selectedId(const QComboBox *combo) const;

	QComboBox *primary_ = nullptr;
	QComboBox *backup_ = nullptr;
	QLabel *status_ = nullptr;
	QTextBrowser *report_ = nullptr;
	QPushButton *saveButton_ = nullptr;
	QPushButton *simulateButton_ = nullptr;
	QPushButton *exportButton_ = nullptr;

	twin::Snapshot current_;
	std::optional<twin::Comparison> comparison_;
	std::optional<twin::Simulation> simulation_;
	bool haveSnapshot_ = false;
	bool updating_ = false;
};
