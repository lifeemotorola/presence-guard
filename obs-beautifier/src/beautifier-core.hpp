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

#include <cstddef>
#include <string>

/*
 * Everything that decides what OBS looks like lives here: colour maths, option
 * clamping and the Qt stylesheet itself. No Qt and no libobs, so the whole look
 * of the plugin can be tested without starting OBS - including the one thing
 * that matters for a UI skin, that text stays readable on its background.
 */
namespace beautify {

enum class Density {
	Compact = 0,
	Normal = 1,
	Comfortable = 2,
};

struct Options {
	bool enabled = false;

	/* False: use accentRgb. True: use the accent of the active OBS theme. */
	bool autoAccent = true;
	int accentRgb = 0xF0A63C;

	int radius = 6;
	Density density = Density::Normal;

	/* Percentage of the application's own font size, 90 - 130. */
	int fontScale = 100;

	bool slimScrollbars = true;
	bool roundedButtons = true;
	bool accentTabs = true;
	bool compactDocks = true;
};

/*
 * The colours OBS is using right now, read from QApplication::palette() so the
 * layer sits on top of whatever theme is active instead of fighting it.
 * Values are 0xRRGGBB.
 */
struct Palette {
	int window = 0x1E1E1E;
	int text = 0xE8E8E8;
	int highlight = 0x3F6FB5;
	int fontSize = 14;
};

/* The colours the stylesheet will use. */
struct Style {
	bool dark = true;

	int base = 0;
	int surface = 0;
	int surfaceAlt = 0;
	int raised = 0;
	int border = 0;
	int borderStrong = 0;
	int text = 0;
	int textDim = 0;
	int accent = 0;
	int accentText = 0;

	/* Resolved from the application font and Options::fontScale. */
	int fontPx = 14;
};

/* --- colour helpers (all values are 0xRRGGBB) --- */

int parseHex(const std::string &value, int fallback);
std::string toHex(int rgb);

int mix(int from, int to, int percent);
int lighten(int rgb, int percent);
int darken(int rgb, int percent);

/* WCAG relative luminance and contrast ratio (1.0 - 21.0). */
double luminance(int rgb);
double contrast(int foreground, int background);

bool isDark(int rgb);

/* Pushes `foreground` towards black or white until it is readable on
 * `background`, and gives up at the closest it can get. */
int readableOn(int foreground, int background, double atLeast = 4.5);

/* --- the look --- */

/* Clamps every field into a sane range; never rejects the whole struct. */
Options normalize(const Options &options);

Style resolve(const Options &options, const Palette &palette);

/*
 * The Qt stylesheet, or an empty string when the plugin is off. The result
 * starts with a marker comment so the caller can strip its own layer back out
 * of QApplication::styleSheet().
 */
std::string styleSheet(const Options &options, const Style &style);

/* The marker the generated sheet starts with. */
extern const char *const kLayerMarker;

/* --- presets --- */

struct Preset {
	const char *id;
	int accentRgb;
	int radius;
	Density density;
	bool accentTabs;
};

/* Returns the built-in presets; count is written to *count. */
const Preset *presets(std::size_t *count);

bool applyPreset(const std::string &id, Options *options);

} // namespace beautify
