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

#include "beautifier.hpp"

#include <plugin-support.h>
#include <util/platform.h>

#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QPalette>
#include <QString>

#include <string>

static const char *SETTINGS_FILE = "settings.json";

Beautifier::Beautifier(QObject *parent) : QObject(parent) {}

Beautifier::~Beautifier()
{
	/* Never leave a plugin's stylesheet behind on a dead QApplication. */
	if (applied_ && qApp)
		revert();
}

/* ------------------------------------------------------------------------- */
/* settings                                                                  */
/* ------------------------------------------------------------------------- */

void Beautifier::loadSettings()
{
	char *path = obs_module_config_path(SETTINGS_FILE);
	if (!path)
		return;

	obs_data_t *data = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);

	if (!data) {
		obs_log(LOG_INFO, "no saved settings yet, using the defaults");
		return;
	}

	options.enabled = obs_data_get_bool(data, "enabled");
	options.autoAccent = obs_data_get_bool(data, "autoAccent");
	options.accentRgb = beautify::parseHex(obs_data_get_string(data, "accent"), options.accentRgb);
	options.radius = (int)obs_data_get_int(data, "radius");
	options.density = (beautify::Density)(int)obs_data_get_int(data, "density");
	options.fontScale = (int)obs_data_get_int(data, "fontScale");
	options.slimScrollbars = obs_data_get_bool(data, "slimScrollbars");
	options.roundedButtons = obs_data_get_bool(data, "roundedButtons");
	options.accentTabs = obs_data_get_bool(data, "accentTabs");
	options.compactDocks = obs_data_get_bool(data, "compactDocks");

	options = beautify::normalize(options);

	obs_data_release(data);

	obs_log(LOG_INFO, "loaded settings; beautify is %s", options.enabled ? "on" : "off");
}

void Beautifier::saveNow() const
{
	obs_data_t *data = obs_data_create();

	obs_data_set_bool(data, "enabled", options.enabled);
	obs_data_set_bool(data, "autoAccent", options.autoAccent);
	const std::string accent = beautify::toHex(options.accentRgb);
	obs_data_set_string(data, "accent", accent.c_str());
	obs_data_set_int(data, "radius", options.radius);
	obs_data_set_int(data, "density", (long long)options.density);
	obs_data_set_int(data, "fontScale", options.fontScale);
	obs_data_set_bool(data, "slimScrollbars", options.slimScrollbars);
	obs_data_set_bool(data, "roundedButtons", options.roundedButtons);
	obs_data_set_bool(data, "accentTabs", options.accentTabs);
	obs_data_set_bool(data, "compactDocks", options.compactDocks);

	char *path = obs_module_config_path(SETTINGS_FILE);
	if (path) {
		/* libobs does not create the per-module config folder for us. */
		char *dir = bstrdup(path);
		char *slash = strrchr(dir, '/');
		if (slash) {
			*slash = '\0';
			os_mkdirs(dir);
		}
		bfree(dir);

		if (!obs_data_save_json_safe(data, path, "tmp", "bak"))
			obs_log(LOG_WARNING, "could not save settings to '%s'", path);

		bfree(path);
	}

	obs_data_release(data);
}

void Beautifier::applyOptions(const beautify::Options &next)
{
	options = beautify::normalize(next);

	apply();
	saveNow();
}

/* ------------------------------------------------------------------------- */
/* the stylesheet                                                            */
/* ------------------------------------------------------------------------- */

beautify::Palette Beautifier::readPalette() const
{
	beautify::Palette palette;

	const QPalette appPalette = QApplication::palette();
	palette.window = appPalette.color(QPalette::Window).rgb() & 0xFFFFFF;
	palette.text = appPalette.color(QPalette::WindowText).rgb() & 0xFFFFFF;
	palette.highlight = appPalette.color(QPalette::Highlight).rgb() & 0xFFFFFF;

	/* The stylesheet needs an absolute size; the font may only have a point
	 * size, so resolve it the way Qt would. */
	const QFont font = QApplication::font();
	int pixelSize = font.pixelSize();
	if (pixelSize <= 0)
		pixelSize = QFontInfo(font).pixelSize();
	palette.fontSize = pixelSize > 0 ? pixelSize : 14;

	return palette;
}

QString Beautifier::themeStyleSheet()
{
	if (!qApp)
		return QString();

	const QString current = qApp->styleSheet();
	const QString marker = QString::fromUtf8(beautify::kLayerMarker);
	const int index = current.indexOf(marker);

	if (index < 0)
		return current;

	/* Everything before the marker is OBS's own sheet. */
	return current.left(index);
}

void Beautifier::apply()
{
	QApplication *app = qApp;
	if (!app)
		return;

	/* Always rebuild from the theme's own sheet: applying twice must not
	 * stack two copies of our layer. */
	QString sheet = themeStyleSheet();

	const beautify::Style style = beautify::resolve(options, readPalette());
	const std::string css = beautify::styleSheet(options, style);

	if (!css.empty()) {
		if (!sheet.isEmpty() && !sheet.endsWith(QChar('\n')))
			sheet += QChar('\n');
		sheet += QString::fromStdString(css);
	}

	app->setStyleSheet(sheet);
	applied_ = !css.empty();

	if (applied_)
		obs_log(LOG_INFO, "beautified OBS: accent %s, radius %d, %s density, %d%% font",
			beautify::toHex(style.accent).c_str(), options.radius,
			options.density == beautify::Density::Compact       ? "compact"
			: options.density == beautify::Density::Comfortable ? "comfortable"
									    : "normal",
			options.fontScale);
	else
		obs_log(LOG_INFO, "removed the beautifier layer, OBS is back to its theme");
}

void Beautifier::revert()
{
	if (!qApp)
		return;

	qApp->setStyleSheet(themeStyleSheet());
	applied_ = false;
}

/* ------------------------------------------------------------------------- */
/* events                                                                    */
/* ------------------------------------------------------------------------- */

void Beautifier::handleEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		loadSettings();
		apply();
		break;

	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		/* OBS has just replaced the application stylesheet, so our layer
		 * is gone with it. Put it back on top of the new theme. */
		if (options.enabled)
			apply();
		else
			applied_ = false;
		break;

	case OBS_FRONTEND_EVENT_EXIT:
		saveNow();
		break;

	default:
		break;
	}
}

/* ------------------------------------------------------------------------- */
/* status                                                                    */
/* ------------------------------------------------------------------------- */

QString Beautifier::statusText() const
{
	if (detached_)
		return QString();

	if (!options.enabled)
		return QString::fromUtf8(obs_module_text("Beautifier.Status.Off"));

	if (!applied_)
		return QString::fromUtf8(obs_module_text("Beautifier.Status.NotApplied"));

	return QString::fromUtf8(obs_module_text("Beautifier.Status.Active"));
}

QString Beautifier::detailText() const
{
	if (detached_)
		return QString();

	const QString accent = QString::fromStdString(beautify::toHex(options.accentRgb));

	if (options.autoAccent)
		return QString::fromUtf8(obs_module_text("Beautifier.Detail.AutoAccent"));

	return QString::fromUtf8(obs_module_text("Beautifier.Detail.Accent")).arg(accent);
}
