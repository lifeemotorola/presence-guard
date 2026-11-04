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

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QObject>
#include <QString>

#include "beautifier-core.hpp"

/*
 * Applies the generated stylesheet to OBS and takes it back off again.
 *
 * The layer is added on top of whatever the active OBS theme put in
 * QApplication::styleSheet(), never instead of it: the theme's own rules keep
 * working underneath, and removing our layer restores OBS exactly as it was.
 * Every block we emit starts with beautify::kLayerMarker, so the theme's sheet
 * can always be recovered from the application's current sheet - including
 * after OBS has replaced it, which is what happens when the theme changes.
 */
class Beautifier : public QObject {
public:
	explicit Beautifier(QObject *parent = nullptr);
	~Beautifier() override;

	beautify::Options options;

	/* Applies (or removes, when disabled) and saves. */
	void applyOptions(const beautify::Options &next);
	void loadSettings();
	void saveNow() const;

	void handleEvent(enum obs_frontend_event event);

	/* Takes the layer off without touching the saved options. */
	void revert();

	bool active() const { return applied_; }

	QString statusText() const;
	QString detailText() const;

	/* The dock may outlive the beautifier during module unload. */
	void detach() { detached_ = true; }

private:
	void apply();
	beautify::Palette readPalette() const;

	/* The application's stylesheet with our layer removed. */
	static QString themeStyleSheet();

	bool applied_ = false;
	bool detached_ = false;
};
