/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Exercises the actual Qt dock against fake OBS data, without a live stream.
Licensed under GPL-2.0-or-later; this program comes WITHOUT ANY WARRANTY.
*/
#include "twin-dock.hpp"

#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <cstdio>
#include <string>

void fakeSetConfigRoot(const std::string &path);
void fakeSetupUiFixture();
void fakeSetMainVisible(bool visible);

static int checks = 0, failures = 0;
static void check(bool value, const char *description)
{
	++checks;
	if (!value) {
		++failures;
		std::printf("FAIL  %s\n", description);
	} else
		std::printf("PASS  %s\n", description);
	std::fflush(stdout);
}

static QPushButton *findButton(TwinDock &dock, const QString &text)
{
	for (QPushButton *button : dock.findChildren<QPushButton *>())
		if (button->text() == text)
			return button;
	return nullptr;
}

int main(int argc, char **argv)
{
	std::printf("Starting QApplication\n");
	std::fflush(stdout);
	QApplication app(argc, argv);
	std::printf("QApplication started\n");
	std::fflush(stdout);
	QString unexpectedDialog;
	QTimer dialogGuard;
	QObject::connect(&dialogGuard, &QTimer::timeout, [&unexpectedDialog]() {
		for (QWidget *widget : QApplication::topLevelWidgets()) {
			auto *box = qobject_cast<QMessageBox *>(widget);
			if (box && box->isVisible()) {
				unexpectedDialog += box->windowTitle() + ": " + box->text();
				box->done(QDialog::Rejected);
			}
		}
	});
	dialogGuard.start(250);
	QTemporaryDir config;
	check(config.isValid(), "temporary plugin config folder created");
	fakeSetConfigRoot(config.path().toUtf8().toStdString());
	fakeSetupUiFixture();
	TwinDock dock;
	dock.resize(300, 650);
	dock.show();
	app.processEvents();

	auto *scan = findButton(dock, "Scan OBS setup");
	auto *save = findButton(dock, "Save current baseline");
	auto *simulate = findButton(dock, "Simulate failure");
	auto *report = dock.findChild<QTextBrowser *>();
	QList<QComboBox *> combos = dock.findChildren<QComboBox *>();
	check(scan && save && simulate && report && combos.size() == 2, "all essential controls exist");
	if (!scan || !save || !simulate || !report || combos.size() != 2)
		return 1;
	check(!save->isEnabled() && !simulate->isEnabled(), "actions stay disabled until a scan");
	scan->click();
	app.processEvents();
	check(save->isEnabled() && simulate->isEnabled(), "scanning enables relevant actions");
	check(combos[0]->count() == 2 && combos[1]->count() == 3, "source lists reflect fake OBS scene");
	check(report->toPlainText().contains("UI fixture"), "report displays the scanned collection");
	const int primaryIndex = combos[0]->findData("ui-cam");
	const int backupIndex = combos[1]->findData("ui-backup");
	check(primaryIndex >= 0 && backupIndex >= 0, "source selection uses stable source IDs");
	combos[0]->setCurrentIndex(primaryIndex);
	combos[1]->setCurrentIndex(backupIndex);
	simulate->click();
	app.processEvents();
	const QString simulationText = report->toPlainText();
	check(simulationText.contains("Main camera") && simulationText.contains("Backup camera"),
	      "failure simulation shows selected sources");
	check(simulationText.contains("0 enabled path(s), 1 disabled path(s)"),
	      "hidden backup is reported as disabled");
	check(simulationText.contains("not a live test"), "UI makes the model limitation visible");

	save->click();
	app.processEvents();
	check(QFileInfo::exists(config.path() + "/baseline-v1.json"), "baseline is saved in plugin config folder");
	fakeSetMainVisible(false);
	scan->click();
	app.processEvents();
	check(report->toPlainText().contains("Main camera path disabled"), "rescan detects baseline visibility drift");

	/* Check both a narrow and a wider dock. The containing scroll area is
	 * essential: controls must remain reachable even at a small dock height. */
	for (int width : {260, 420}) {
		dock.resize(width, 500);
		app.processEvents();
		bool fits = true;
		for (QPushButton *button : dock.findChildren<QPushButton *>()) {
			const int textWidth = QFontMetrics(button->font()).horizontalAdvance(button->text());
			if (button->width() < textWidth + 12)
				fits = false;
		}
		check(fits, width == 260 ? "narrow dock buttons do not clip their labels"
					 : "wide dock buttons do not clip their labels");
	}
	auto *scroll = dock.findChild<QScrollArea *>();
	check(scroll && scroll->widgetResizable(), "dock controls remain reachable in a resizable scroll area");
	const QByteArray screenshotPath = qgetenv("ONAIRTWIN_UI_SCREENSHOT");
	if (!screenshotPath.isEmpty()) {
		dock.resize(420, 900);
		app.processEvents();
		check(dock.grab().save(QString::fromUtf8(screenshotPath)), "UI screenshot captured");
	}
	if (!unexpectedDialog.isEmpty())
		std::printf("Unexpected dialog: %s\n", unexpectedDialog.toUtf8().constData());
	check(unexpectedDialog.isEmpty(), "no unexpected error or confirmation dialogs occurred");
	std::printf("\n%d UI checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
