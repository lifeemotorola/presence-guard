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

#include "beautifier-core.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace beautify {

const char *const kLayerMarker = "/* obs-beautifier layer */";

namespace {

int clampInt(int value, int low, int high)
{
	if (value < low)
		return low;
	if (value > high)
		return high;
	return value;
}

double channel(int rgb, int shift)
{
	return (double)((rgb >> shift) & 0xFF) / 255.0;
}

double linearize(double value)
{
	if (value <= 0.03928)
		return value / 12.92;
	return std::pow((value + 0.055) / 1.055, 2.4);
}

int roundChannel(double value)
{
	return clampInt((int)std::lround(value), 0, 255);
}

int padTop(const Density density)
{
	switch (density) {
	case Density::Compact:
		return 3;
	case Density::Comfortable:
		return 7;
	case Density::Normal:
	default:
		return 5;
	}
}

int padSide(const Density density)
{
	switch (density) {
	case Density::Compact:
		return 9;
	case Density::Comfortable:
		return 16;
	case Density::Normal:
	default:
		return 12;
	}
}

int rowPad(const Density density)
{
	switch (density) {
	case Density::Compact:
		return 3;
	case Density::Comfortable:
		return 7;
	case Density::Normal:
	default:
		return 5;
	}
}

std::string px(int value)
{
	return std::to_string(value) + "px";
}

/* One QSS rule: selector { body }. */
void rule(std::string &css, const std::string &selector, const std::string &body)
{
	css += selector;
	css += " {\n    ";
	css += body;
	css += "\n}\n";
}

} // namespace

/* ------------------------------------------------------------------------- */
/* colour maths                                                              */
/* ------------------------------------------------------------------------- */

int parseHex(const std::string &value, int fallback)
{
	std::string digits;

	for (char c : value) {
		if (c == '#' || c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;
		if (!std::isxdigit((unsigned char)c))
			return fallback;
		digits.push_back(c);
	}

	/* #abc is shorthand for #aabbcc. */
	if (digits.size() == 3)
		digits = {digits[0], digits[0], digits[1], digits[1], digits[2], digits[2]};

	if (digits.size() != 6)
		return fallback;

	return (int)std::strtol(digits.c_str(), nullptr, 16) & 0xFFFFFF;
}

std::string toHex(int rgb)
{
	char buffer[8];
	std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
	return std::string(buffer);
}

int mix(int from, int to, int percent)
{
	percent = clampInt(percent, 0, 100);

	if (percent == 0)
		return from & 0xFFFFFF;
	if (percent == 100)
		return to & 0xFFFFFF;

	const double t = (double)percent / 100.0;
	const int red = roundChannel((double)((from >> 16) & 0xFF) * (1.0 - t) + (double)((to >> 16) & 0xFF) * t);
	const int green = roundChannel((double)((from >> 8) & 0xFF) * (1.0 - t) + (double)((to >> 8) & 0xFF) * t);
	const int blue = roundChannel((double)(from & 0xFF) * (1.0 - t) + (double)(to & 0xFF) * t);

	return (red << 16) | (green << 8) | blue;
}

int lighten(int rgb, int percent)
{
	return mix(rgb, 0xFFFFFF, percent);
}

int darken(int rgb, int percent)
{
	return mix(rgb, 0x000000, percent);
}

double luminance(int rgb)
{
	const double red = linearize(channel(rgb, 16));
	const double green = linearize(channel(rgb, 8));
	const double blue = linearize(channel(rgb, 0));

	return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

double contrast(int foreground, int background)
{
	const double a = luminance(foreground);
	const double b = luminance(background);
	const double lighter = std::max(a, b);
	const double darker = std::min(a, b);

	return (lighter + 0.05) / (darker + 0.05);
}

bool isDark(int rgb)
{
	return luminance(rgb) < 0.35;
}

int readableOn(int foreground, int background, double atLeast)
{
	if (contrast(foreground, background) >= atLeast)
		return foreground & 0xFFFFFF;

	int best = foreground & 0xFFFFFF;
	double bestRatio = contrast(best, background);

	for (int step = 1; step <= 20; step++) {
		const int targets[2] = {0xFFFFFF, 0x000000};

		for (int target : targets) {
			const int candidate = mix(foreground, target, step * 5);
			const double ratio = contrast(candidate, background);

			if (ratio >= atLeast)
				return candidate;
			if (ratio > bestRatio) {
				bestRatio = ratio;
				best = candidate;
			}
		}
	}

	return best;
}

/* ------------------------------------------------------------------------- */
/* the look                                                                  */
/* ------------------------------------------------------------------------- */

Options normalize(const Options &options)
{
	Options out = options;

	out.accentRgb &= 0xFFFFFF;
	out.radius = clampInt(out.radius, 0, 20);
	out.fontScale = clampInt(out.fontScale, 90, 130);

	int density = (int)out.density;
	if (density < 0)
		density = 0;
	if (density > 2)
		density = 2;
	out.density = (Density)density;

	return out;
}

Style resolve(const Options &options, const Palette &palette)
{
	const Options o = normalize(options);

	Style style;
	style.dark = isDark(palette.window);

	style.base = palette.window & 0xFFFFFF;

	/* Panels, inputs and borders step away from the window colour towards
	 * white on dark themes and towards black on light ones, so the same
	 * rules work for both. */
	const int step = style.dark ? 0xFFFFFF : 0x000000;
	style.surface = mix(style.base, step, style.dark ? 7 : 4);
	style.surfaceAlt = mix(style.base, step, style.dark ? 12 : 7);
	style.raised = mix(style.base, step, style.dark ? 18 : 11);
	style.border = mix(style.base, step, style.dark ? 18 : 16);
	style.borderStrong = mix(style.base, step, style.dark ? 32 : 28);

	style.text = readableOn(palette.text, style.base, 8.0);
	style.textDim = readableOn(mix(style.text, style.base, 42), style.base, 4.6);

	const int accent = o.autoAccent ? palette.highlight : o.accentRgb;
	style.accent = readableOn(accent, style.base, 3.0);

	/* Text on the accent has to survive the accent being anywhere between
	 * almost black and almost white. */
	const int darkInk = 0x101010;
	const int lightInk = 0xFFFFFF;
	const int ink = contrast(darkInk, style.accent) >= contrast(lightInk, style.accent) ? darkInk : lightInk;
	style.accentText = readableOn(ink, style.accent, 4.6);

	const int fontPx =
		clampInt((int)std::lround((double)clampInt(palette.fontSize, 8, 32) * o.fontScale / 100.0), 8, 42);
	style.fontPx = fontPx;

	return style;
}

std::string styleSheet(const Options &options, const Style &style)
{
	const Options o = normalize(options);
	if (!o.enabled)
		return std::string();

	const int radius = o.roundedButtons ? o.radius : 0;
	const int small = std::max(1, radius - 2);
	const int top = padTop(o.density);
	const int side = padSide(o.density);
	const int row = rowPad(o.density);

	const std::string base = toHex(style.base);
	const std::string surface = toHex(style.surface);
	const std::string surfaceAlt = toHex(style.surfaceAlt);
	const std::string raised = toHex(style.raised);
	const std::string border = toHex(style.border);
	const std::string borderStrong = toHex(style.borderStrong);
	const std::string text = toHex(style.text);
	const std::string textDim = toHex(style.textDim);
	const std::string accent = toHex(style.accent);
	const std::string accentText = toHex(style.accentText);
	const std::string buttonPad = px(top) + " " + px(side);

	std::string css;
	css += kLayerMarker;
	css += "\n";

	/* Text and font everywhere. Deliberately no background here: painting
	 * every QWidget would hide the widgets that draw themselves. */
	rule(css, "QWidget", "color: " + text + ";\n    font-size: " + px(style.fontPx) + ";");

	rule(css, "QMainWindow, QDialog", "background-color: " + base + ";");

	rule(css, "QToolTip",
	     "background-color: " + surfaceAlt + ";\n    color: " + text + ";\n    border: 1px solid " + borderStrong +
		     ";\n    border-radius: " + px(small) + ";\n    padding: 4px 8px;");

	/* --- menu bar and menus --- */
	rule(css, "QMenuBar", "background-color: " + base + ";\n    border: none;");
	rule(css, "QMenuBar::item",
	     "background: transparent;\n    padding: 4px 10px;\n    margin: 2px 1px;\n    border-radius: " + px(small) +
		     ";");
	rule(css, "QMenuBar::item:selected, QMenuBar::item:pressed", "background-color: " + raised + ";");
	rule(css, "QMenu",
	     "background-color: " + surface + ";\n    border: 1px solid " + border +
		     ";\n    border-radius: " + px(radius) + ";\n    padding: 4px;");
	rule(css, "QMenu::item",
	     "padding: " + px(row) + " 26px " + px(row) + " 12px;\n    border-radius: " + px(small) + ";");
	rule(css, "QMenu::item:selected", "background-color: " + accent + ";\n    color: " + accentText + ";");
	rule(css, "QMenu::item:disabled", "color: " + textDim + ";");
	rule(css, "QMenu::separator", "height: 1px;\n    background-color: " + border + ";\n    margin: 5px 8px;");

	/* --- buttons --- */
	rule(css, "QPushButton",
	     "background-color: " + surfaceAlt + ";\n    color: " + text + ";\n    border: 1px solid " + border +
		     ";\n    border-radius: " + px(radius) + ";\n    padding: " + buttonPad +
		     ";\n    min-height: 14px;");
	rule(css, "QPushButton:hover", "background-color: " + raised + ";\n    border-color: " + borderStrong + ";");
	rule(css, "QPushButton:pressed", "background-color: " + surface + ";\n    border-color: " + accent + ";");
	rule(css, "QPushButton:focus, QPushButton:default", "border-color: " + accent + ";\n    outline: none;");
	rule(css, "QPushButton:disabled", "color: " + textDim + ";\n    border-color: " + border + ";");

	rule(css, "QToolButton",
	     "background: transparent;\n    border: 1px solid transparent;\n    border-radius: " + px(small) +
		     ";\n    padding: 3px;");
	rule(css, "QToolButton:hover", "background-color: " + raised + ";\n    border-color: " + border + ";");
	rule(css, "QToolButton:pressed, QToolButton:checked",
	     "background-color: " + surfaceAlt + ";\n    border-color: " + accent + ";");

	/* --- inputs --- */
	rule(css, "QLineEdit, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextEdit",
	     "background-color: " + surfaceAlt + ";\n    color: " + text + ";\n    border: 1px solid " + border +
		     ";\n    border-radius: " + px(small) + ";\n    padding: " + buttonPad +
		     ";\n    selection-background-color: " + accent + ";\n    selection-color: " + accentText + ";");
	rule(css, "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QPlainTextEdit:focus, QTextEdit:focus",
	     "border-color: " + accent + ";");
	rule(css, "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button",
	     "width: 16px;\n    border: none;");
	rule(css, "QComboBox",
	     "background-color: " + surfaceAlt + ";\n    color: " + text + ";\n    border: 1px solid " + border +
		     ";\n    border-radius: " + px(small) + ";\n    padding: " + buttonPad + ";");
	rule(css, "QComboBox:hover, QComboBox:focus", "border-color: " + accent + ";");
	rule(css, "QComboBox::drop-down", "border: none;\n    width: 18px;");
	rule(css, "QComboBox QAbstractItemView",
	     "background-color: " + surface + ";\n    color: " + text + ";\n    border: 1px solid " + border +
		     ";\n    border-radius: " + px(small) + ";\n    selection-background-color: " + accent +
		     ";\n    selection-color: " + accentText + ";\n    outline: none;");

	rule(css, "QCheckBox, QRadioButton", "color: " + text + ";\n    spacing: 6px;");
	rule(css, "QCheckBox::indicator, QRadioButton::indicator",
	     "width: 14px;\n    height: 14px;\n    border: 1px solid " + borderStrong + ";\n    border-radius: " +
		     px(std::max(1, small - 1)) + ";\n    background-color: " + surfaceAlt + ";");
	rule(css, "QRadioButton::indicator", "border-radius: 8px;");
	rule(css, "QCheckBox::indicator:checked, QRadioButton::indicator:checked",
	     "background-color: " + accent + ";\n    border-color: " + accent + ";");
	rule(css, "QCheckBox::indicator:hover, QRadioButton::indicator:hover", "border-color: " + accent + ";");

	/* --- sliders and progress --- */
	rule(css, "QSlider::groove:horizontal",
	     "height: 6px;\n    background-color: " + surfaceAlt + ";\n    border-radius: 3px;");
	rule(css, "QSlider::sub-page:horizontal", "background-color: " + accent + ";\n    border-radius: 3px;");
	rule(css, "QSlider::handle:horizontal",
	     "background-color: " + text +
		     ";\n    border: none;\n    width: 14px;\n    margin: -5px 0;\n    border-radius: 7px;");
	rule(css, "QSlider::handle:horizontal:hover", "background-color: " + accent + ";");
	rule(css, "QSlider::groove:vertical",
	     "width: 6px;\n    background-color: " + surfaceAlt + ";\n    border-radius: 3px;");
	rule(css, "QSlider::add-page:vertical", "background-color: " + accent + ";\n    border-radius: 3px;");
	rule(css, "QSlider::handle:vertical",
	     "background-color: " + text +
		     ";\n    border: none;\n    height: 14px;\n    margin: 0 -5px;\n    border-radius: 7px;");
	rule(css, "QProgressBar",
	     "background-color: " + surfaceAlt + ";\n    color: " + text + ";\n    border: 1px solid " + border +
		     ";\n    border-radius: " + px(small) + ";\n    text-align: center;");
	rule(css, "QProgressBar::chunk", "background-color: " + accent + ";\n    border-radius: " + px(small) + ";");

	/* --- tabs --- */
	rule(css, "QTabWidget::pane",
	     "border: 1px solid " + border + ";\n    border-radius: " + px(radius) + ";\n    top: -1px;");
	rule(css, "QTabBar::tab",
	     "background: transparent;\n    color: " + textDim + ";\n    padding: " + px(top) + " " + px(side) +
		     ";\n    margin-right: 2px;\n    border: none;");
	rule(css, "QTabBar::tab:hover", "color: " + text + ";");
	if (o.accentTabs) {
		rule(css, "QTabBar::tab:selected",
		     "color: " + text + ";\n    border-bottom: 2px solid " + accent + ";");
	} else {
		rule(css, "QTabBar::tab:selected", "color: " + text + ";");
	}
	rule(css, "QTabBar::tab:!selected", "margin-top: 2px;");

	/* --- scrollbars --- */
	if (o.slimScrollbars) {
		rule(css, "QScrollBar:vertical",
		     "background: transparent;\n    width: 10px;\n    margin: 0;\n    border: none;");
		rule(css, "QScrollBar::handle:vertical",
		     "background-color: " + borderStrong + ";\n    min-height: 24px;\n    border-radius: 5px;");
		rule(css, "QScrollBar:horizontal",
		     "background: transparent;\n    height: 10px;\n    margin: 0;\n    border: none;");
		rule(css, "QScrollBar::handle:horizontal",
		     "background-color: " + borderStrong + ";\n    min-width: 24px;\n    border-radius: 5px;");
		rule(css, "QScrollBar::handle:hover", "background-color: " + accent + ";");
		rule(css, "QScrollBar::add-line, QScrollBar::sub-line", "width: 0;\n    height: 0;\n    border: none;");
		rule(css, "QScrollBar::add-page, QScrollBar::sub-page", "background: transparent;");
	}

	/* --- containers --- */
	rule(css, "QGroupBox",
	     "border: 1px solid " + border + ";\n    border-radius: " + px(radius) +
		     ";\n    margin-top: 10px;\n    padding-top: 6px;");
	rule(css, "QGroupBox::title",
	     "subcontrol-origin: margin;\n    left: 10px;\n    padding: 0 4px;\n    color: " + textDim + ";");

	if (o.compactDocks) {
		rule(css, "QDockWidget", "border: 1px solid " + border + ";");
		rule(css, "QDockWidget::title",
		     "background-color: " + surface +
			     ";\n    padding: 4px 8px;\n    text-align: left;\n    border-bottom: 1px solid " + border +
			     ";");
		rule(css, "QDockWidget > QWidget", "background-color: " + surface + ";");
	}

	rule(css, "QHeaderView::section",
	     "background-color: " + surface + ";\n    color: " + textDim +
		     ";\n    border: none;\n    border-bottom: 1px solid " + border + ";\n    padding: " + px(row) +
		     " " + px(6) + ";");
	rule(css, "QTreeView, QListView, QTableView, QTableWidget",
	     "background-color: " + surfaceAlt + ";\n    alternate-background-color: " + surface +
		     ";\n    border: 1px solid " + border + ";\n    border-radius: " + px(small) +
		     ";\n    outline: none;");
	rule(css, "QTreeView::item:selected, QListView::item:selected, QTableView::item:selected",
	     "background-color: " + accent + ";\n    color: " + accentText + ";");
	rule(css, "QTreeView::item:hover, QListView::item:hover", "background-color: " + raised + ";");

	rule(css, "QStatusBar", "background-color: " + base + ";\n    color: " + textDim + ";");
	rule(css, "QStatusBar::item", "border: none;");
	rule(css, "QSplitter::handle", "background-color: " + border + ";");
	rule(css, "QScrollArea", "border: none;");

	return css;
}

/* ------------------------------------------------------------------------- */
/* presets                                                                   */
/* ------------------------------------------------------------------------- */

namespace {

const Preset kPresets[] = {
	{"amber", 0xF0A63C, 6, Density::Normal, true},    {"graphite", 0x9AA4B2, 4, Density::Compact, false},
	{"nord", 0x5E81AC, 8, Density::Normal, true},     {"lagoon", 0x2FBFA0, 8, Density::Comfortable, true},
	{"magenta", 0xE8559B, 10, Density::Normal, true}, {"contrast", 0xFFD400, 0, Density::Compact, false},
};

} // namespace

const Preset *presets(std::size_t *count)
{
	if (count)
		*count = sizeof(kPresets) / sizeof(kPresets[0]);

	return kPresets;
}

bool applyPreset(const std::string &id, Options *options)
{
	if (!options)
		return false;

	for (const Preset &preset : kPresets) {
		if (id != preset.id)
			continue;

		options->autoAccent = false;
		options->accentRgb = preset.accentRgb;
		options->radius = preset.radius;
		options->density = preset.density;
		options->accentTabs = preset.accentTabs;

		return true;
	}

	return false;
}

} // namespace beautify
