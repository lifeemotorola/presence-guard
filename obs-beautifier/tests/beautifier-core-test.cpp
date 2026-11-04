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

/*
 * The look of the plugin is a pure function of the options and the palette
 * OBS is using, so it is tested here without Qt: colour maths, option
 * clamping, and the properties that matter for a skin - readable contrast and
 * a stylesheet Qt will not choke on. Build with tests/run-tests.sh.
 */

#include "beautifier-core.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

static void check(bool condition, const char *what)
{
	g_checks++;
	if (condition) {
		std::printf("  PASS  %s\n", what);
		return;
	}

	g_failures++;
	std::printf("  FAIL  %s\n", what);
}

using namespace beautify;

static Palette darkPalette()
{
	Palette palette;
	palette.window = 0x1E1E1E;
	palette.text = 0xE8E8E8;
	palette.highlight = 0x3F6FB5;
	palette.fontSize = 14;
	return palette;
}

static Palette lightPalette()
{
	Palette palette;
	palette.window = 0xF2F2F2;
	palette.text = 0x1C1C1C;
	palette.highlight = 0x3F6FB5;
	palette.fontSize = 12;
	return palette;
}

static Options enabledOptions()
{
	Options options;
	options.enabled = true;
	return options;
}

static bool balancedBraces(const std::string &text)
{
	int depth = 0;

	for (char c : text) {
		if (c == '{')
			depth++;
		else if (c == '}')
			depth--;

		if (depth < 0)
			return false;
	}

	return depth == 0;
}

/* Every "#rrggbb" in the sheet has to be six hex digits. */
static bool coloursWellFormed(const std::string &text)
{
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] != '#')
			continue;
		if (i + 6 >= text.size())
			return false;

		for (size_t j = 1; j <= 6; j++) {
			const char c = text[i + j];
			const bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
			if (!hex)
				return false;
		}

		/* A seventh digit would mean a malformed colour. */
		if (i + 7 < text.size()) {
			const char c = text[i + 7];
			const bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
			if (hex)
				return false;
		}
	}

	return true;
}

static bool contains(const std::string &haystack, const std::string &needle)
{
	return haystack.find(needle) != std::string::npos;
}

int main()
{
	std::printf("colour parsing\n");
	{
		check(toHex(0xF0A63C) == "#F0A63C", "toHex formats six digits");
		check(toHex(0x00FF7F) == "#00FF7F", "toHex pads every channel");
		check(parseHex("#F0A63C", 0) == 0xF0A63C, "parseHex reads the # form");
		check(parseHex("f0a63c", 0) == 0xF0A63C, "parseHex reads without the #");
		check(parseHex("#abc", 0) == 0xAABBCC, "parseHex expands the shorthand form");
		check(parseHex("  #ABC  ", 0) == 0xAABBCC, "parseHex ignores surrounding space");
		check(parseHex("16-bit", 0x123456) == 0x123456, "parseHex rejects junk");
		check(parseHex("#12345", 0x123456) == 0x123456, "parseHex rejects the wrong length");
		check(parseHex(toHex(0x0A1B2C), 0) == 0x0A1B2C, "toHex and parseHex round trip");
	}

	std::printf("colour maths\n");
	{
		check(mix(0x000000, 0xFFFFFF, 0) == 0x000000, "mix at 0% keeps the start");
		check(mix(0x000000, 0xFFFFFF, 100) == 0xFFFFFF, "mix at 100% reaches the end");
		check(mix(0x000000, 0xFFFFFF, 50) == 0x808080, "mix at 50% is the midpoint");
		check(mix(0x000000, 0xFFFFFF, 140) == 0xFFFFFF, "mix clamps above 100%");
		check(mix(0x000000, 0xFFFFFF, -20) == 0x000000, "mix clamps below 0%");

		check(luminance(0x000000) < 0.001, "black is almost zero luminance");
		check(std::fabs(luminance(0xFFFFFF) - 1.0) < 0.001, "white is full luminance");
		check(luminance(0x808080) > luminance(0x404040), "luminance orders greys");

		check(std::fabs(contrast(0x000000, 0xFFFFFF) - 21.0) < 0.05, "black on white is 21:1");
		check(std::fabs(contrast(0x123456, 0x123456) - 1.0) < 0.001, "a colour against itself is 1:1");
		check(std::fabs(contrast(0xFFFFFF, 0x000000) - contrast(0x000000, 0xFFFFFF)) < 0.001,
		      "contrast is symmetric");

		check(lighten(0x101010, 50) == mix(0x101010, 0xFFFFFF, 50), "lighten is a mix towards white");
		check(darken(0xF0F0F0, 50) == mix(0xF0F0F0, 0x000000, 50), "darken is a mix towards black");

		check(isDark(0x101010), "dark colours are dark");
		check(!isDark(0xF0F0F0), "light colours are not dark");
	}

	std::printf("readable text\n");
	{
		check(contrast(readableOn(0x333333, 0x1E1E1E, 4.5), 0x1E1E1E) >= 4.5,
		      "dark on dark is pushed until readable");
		check(contrast(readableOn(0x777777, 0xF2F2F2, 4.5), 0xF2F2F2) >= 4.5,
		      "light grey on white is pushed until readable");
		check(readableOn(0xFFFFFF, 0x101010, 4.5) == 0xFFFFFF, "already readable text is left alone");
		check(contrast(readableOn(0x777777, 0x777777, 4.5), 0x777777) > 1.5,
		      "even hopeless pairs get the best available contrast");
	}

	std::printf("option clamping\n");
	{
		Options wild;
		wild.accentRgb = 0x1FFFFFF;
		wild.radius = 999;
		wild.fontScale = 400;
		wild.density = (Density)9;

		const Options fixed = normalize(wild);
		check(fixed.accentRgb == 0xFFFFFF, "the accent is masked to 24 bits");
		check(fixed.radius == 20, "the radius is capped");
		check(fixed.fontScale == 130, "the font scale is capped");
		check(fixed.density == Density::Comfortable, "the density is clamped to the last preset");

		Options tiny;
		tiny.radius = -5;
		tiny.fontScale = 10;
		tiny.density = (Density)-3;

		const Options fixedTiny = normalize(tiny);
		check(fixedTiny.radius == 0, "the radius has a floor");
		check(fixedTiny.fontScale == 90, "the font scale has a floor");
		check(fixedTiny.density == Density::Compact, "the density has a floor");
	}

	std::printf("resolving a dark palette\n");
	{
		const Options options = enabledOptions();
		const Style style = resolve(options, darkPalette());

		check(style.dark, "a dark window colour is detected");
		check(style.base == 0x1E1E1E, "the window colour is kept");
		check(luminance(style.surface) > luminance(style.base), "panels are lighter than the window");
		check(luminance(style.surfaceAlt) > luminance(style.surface), "inputs are lighter than panels");
		check(luminance(style.raised) > luminance(style.surfaceAlt), "hover is lighter than inputs");
		check(luminance(style.borderStrong) > luminance(style.border), "strong borders are brighter");

		check(contrast(style.text, style.base) >= 8.0, "body text has at least 8:1 on the window");
		check(contrast(style.text, style.surface) >= 7.0, "body text stays readable on panels");
		check(contrast(style.textDim, style.base) >= 4.5, "dim text still passes WCAG AA");
		check(contrast(style.accent, style.base) >= 3.0, "the accent is visible on the window");
		check(contrast(style.accentText, style.accent) >= 4.5, "text on the accent passes WCAG AA");
		check(style.textDim != style.text, "dim text is actually dimmer");
	}

	std::printf("resolving a light palette\n");
	{
		const Style style = resolve(enabledOptions(), lightPalette());

		check(!style.dark, "a light window colour is detected");
		check(luminance(style.surface) < luminance(style.base), "panels are darker than the window");
		check(contrast(style.text, style.base) >= 8.0, "body text has at least 8:1 on a light window");
		check(contrast(style.text, style.surfaceAlt) >= 7.0, "body text stays readable on inputs");
		check(contrast(style.accentText, style.accent) >= 4.5, "text on the accent passes WCAG AA");
	}

	std::printf("where the accent comes from\n");
	{
		const Palette palette = darkPalette();

		Options autoAccent = enabledOptions();
		autoAccent.autoAccent = true;
		check(resolve(autoAccent, palette).accent == palette.highlight,
		      "theme accent is used when it is already visible");

		Options manual = enabledOptions();
		manual.autoAccent = false;
		manual.accentRgb = 0x2FBFA0;
		check(resolve(manual, palette).accent == 0x2FBFA0, "a chosen accent is used as is when it is visible");

		/* An accent that disappears into the background has to be lifted. */
		Options invisible = enabledOptions();
		invisible.autoAccent = false;
		invisible.accentRgb = 0x212121;
		const Style lifted = resolve(invisible, palette);
		check(lifted.accent != 0x212121, "an accent too close to the window is corrected");
		check(contrast(lifted.accent, lifted.base) >= 3.0, "the corrected accent is visible");
	}

	std::printf("font size\n");
	{
		Palette palette = darkPalette();
		palette.fontSize = 14;

		Options options = enabledOptions();
		check(resolve(options, palette).fontPx == 14, "100% keeps the application font size");

		options.fontScale = 130;
		check(resolve(options, palette).fontPx == 18, "130% of 14 px is 18 px");

		options.fontScale = 90;
		check(resolve(options, palette).fontPx == 13, "90% of 14 px is 13 px");

		palette.fontSize = 4;
		options.fontScale = 90;
		check(resolve(options, palette).fontPx >= 8, "a tiny font is never scaled below 8 px");
	}

	std::printf("the stylesheet\n");
	{
		Options options = enabledOptions();
		const Style style = resolve(options, darkPalette());

		check(styleSheet(Options(), style).empty(), "a disabled plugin produces no stylesheet");

		const std::string css = styleSheet(options, style);
		check(!css.empty(), "an enabled plugin produces a stylesheet");
		check(css.compare(0, std::string(kLayerMarker).size(), kLayerMarker) == 0,
		      "the sheet starts with the marker the plugin strips by");
		check(balancedBraces(css), "every rule is closed");
		check(coloursWellFormed(css), "every colour is a real six digit hex value");
		check(css.find("{{") == std::string::npos, "no rule is emitted twice");

		std::vector<std::string> missing;
		for (const char *selector :
		     {"QMainWindow", "QPushButton", "QComboBox", "QTabBar::tab", "QScrollBar", "QMenu::item",
		      "QGroupBox::title", "QDockWidget::title", "QSlider::handle:horizontal", "QCheckBox::indicator",
		      "QToolTip", "QStatusBar", "QTreeView::item:selected"}) {
			if (!contains(css, selector))
				missing.push_back(selector);
		}
		for (const std::string &selector : missing)
			std::printf("        missing selector: %s\n", selector.c_str());
		check(missing.empty(), "the sheet styles every major widget family");

		check(contains(css, toHex(style.accent)), "the accent colour is used");
		check(contains(css, toHex(style.accentText)), "the accent text colour is used");
		check(contains(css, toHex(style.base)), "the window colour is used");
		check(contains(css, toHex(style.surfaceAlt)), "the input colour is used");
		check(contains(css, "font-size: " + std::to_string(style.fontPx) + "px"), "the font size is applied");

		/* Nothing may leak the application's own styling into the rules. */
		check(css.find("font-family") == std::string::npos, "the font family is left alone");
		check(css.find("QWidget {") != std::string::npos, "the text colour is set application wide");

		const std::string again = styleSheet(options, resolve(options, darkPalette()));
		check(again == css, "the same options always give the same sheet");
	}

	std::printf("options change the sheet\n");
	{
		Palette palette = darkPalette();

		Options plain = enabledOptions();
		plain.slimScrollbars = false;
		check(!contains(styleSheet(plain, resolve(plain, palette)), "QScrollBar"),
		      "slim scrollbars can be switched off");

		Options tabs = enabledOptions();
		tabs.accentTabs = false;
		const std::string noAccentTabs = styleSheet(tabs, resolve(tabs, palette));
		check(!contains(noAccentTabs, "QTabBar::tab:selected {\n    color: " +
						      toHex(resolve(tabs, palette).text) + ";\n    border-bottom"),
		      "the accent underline on tabs can be switched off");
		check(!contains(noAccentTabs, "border-bottom: 2px solid"), "no tab underline is emitted");

		Options docks = enabledOptions();
		docks.compactDocks = false;
		check(!contains(styleSheet(docks, resolve(docks, palette)), "QDockWidget::title"),
		      "the compact dock titles can be switched off");

		Options square = enabledOptions();
		square.roundedButtons = false;
		const std::string flat = styleSheet(square, resolve(square, palette));
		check(contains(flat, "QPushButton {\n    background-color: "), "buttons are still styled when square");
		check(contains(flat, "border-radius: 0px"), "square buttons have no radius");

		Options round = enabledOptions();
		round.radius = 12;
		check(contains(styleSheet(round, resolve(round, palette)), "border-radius: 12px"),
		      "the radius option reaches the sheet");

		Options compact = enabledOptions();
		compact.density = Density::Compact;
		Options comfy = enabledOptions();
		comfy.density = Density::Comfortable;
		check(styleSheet(compact, resolve(compact, palette)) != styleSheet(comfy, resolve(comfy, palette)),
		      "density changes the padding");

		Options big = enabledOptions();
		big.fontScale = 130;
		check(contains(styleSheet(big, resolve(big, palette)), "font-size: 18px"),
		      "the font scale reaches the sheet");
	}

	std::printf("presets\n");
	{
		std::size_t count = 0;
		const Preset *list = presets(&count);
		check(count >= 4, "there are several presets to choose from");
		check(list != nullptr, "the preset list exists");

		bool allApply = true;
		for (std::size_t i = 0; i < count; i++) {
			Options options = enabledOptions();
			if (!applyPreset(list[i].id, &options)) {
				allApply = false;
				continue;
			}
			if (options.accentRgb != list[i].accentRgb || options.radius != list[i].radius) {
				allApply = false;
				continue;
			}
			/* A preset has to survive normalisation untouched. */
			const Options fixed = normalize(options);
			if (fixed.accentRgb != options.accentRgb || fixed.radius != options.radius ||
			    fixed.density != options.density) {
				allApply = false;
			}
		}
		check(allApply, "every preset applies and stays valid");

		Options options = enabledOptions();
		options.radius = 3;
		const Options before = options;
		check(!applyPreset("no-such-preset", &options), "an unknown preset is rejected");
		check(options.radius == before.radius, "a rejected preset changes nothing");
		check(!applyPreset("amber", nullptr), "applying to nothing is rejected");

		Options amber = enabledOptions();
		check(applyPreset("amber", &amber) && !amber.autoAccent, "a preset takes over from the theme accent");
	}

	std::printf("\n%s (%d checks, %d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_checks,
		    g_failures, g_failures == 1 ? "" : "s");

	return g_failures == 0 ? 0 : 1;
}
