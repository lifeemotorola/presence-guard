/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
Licensed under GPL-2.0-or-later; see LICENSE. No warranty.
*/
#pragma once

#include "obs-mixer.hpp"

#include <QMap>
#include <QStringList>
#include <QWidget>

class QLabel;
class QTimer;
class QVBoxLayout;
class SourceCard;

class MixDock : public QWidget {
public:
	explicit MixDock(QWidget *parent = nullptr);
	~MixDock() override;

	/* Called on the GUI thread. The timer also picks up source and OBS edits. */
	void refreshNow();
	void suspendForCollectionChange();
	void resumeAfterCollectionChange();

private:
	void buildUi();
	void connectCard(SourceCard *card);
	void updateMeters();
	void clearCards();
	void reportResult(mixbloom::Result result);

	QVBoxLayout *sourceLayout_ = nullptr;
	QWidget *sourceList_ = nullptr;
	QWidget *empty_ = nullptr;
	QLabel *count_ = nullptr;
	QLabel *notice_ = nullptr;
	QTimer *refreshTimer_ = nullptr;
	QTimer *meterTimer_ = nullptr;
	QMap<QString, SourceCard *> cards_;
	QStringList order_;
	bool suspended_ = false;
};
