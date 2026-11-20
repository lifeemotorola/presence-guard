/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
Licensed under GPL-2.0-or-later; see LICENSE. No warranty.
*/
#pragma once

#include <QDialog>

/* A modeless, offline guide. It never reads or changes OBS audio settings. */
class MixHelpDialog final : public QDialog {
public:
	explicit MixHelpDialog(QWidget *parent = nullptr);
};
